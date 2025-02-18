import csv
import sys


# returns people list, wants, and wants count
def load_things(path: str) -> tuple[list[str], dict[str, list[str]]]:
    og_people_list: list[str] = []
    og_genders: dict[str, str] = {}
    og_wants: dict[str, list[str]] = {}
    with open(path, newline="") as f:
        reader = csv.reader(f)
        for i, line in enumerate(reader):
            if i == 0:
                continue  # FIXME: should cope with reordering columns in the data
            name: str = line[0].lower().strip()
            gender: str = line[1].lower().strip()
            wants: list[str] = line[2].lower().split(", ")
            wants = [x.strip() for x in wants]

            og_people_list.append(name)
            og_genders[name] = gender
            og_wants[name] = wants

    for person in og_people_list:
        gender = og_genders[person]
        bad: list[str] = []
        for other in og_wants[person]:
            if other not in og_people_list:
                print(f"invalid want from {person} to {other}")
                bad.append(other)
                continue
            elif og_genders[other] != gender:
                print(f"invalid want from {person} to {other}")
                bad.append(other)
                continue

        for x in bad:
            og_wants[person].remove(x)

    return og_people_list, og_wants


def gen_strong_pairs_wants_counts_and_adjacents(
    og_people_list: list[str],
    og_wants: dict[str, list[str]],
):
    og_wants_counts: dict[str, int] = {}
    for person in og_people_list:
        og_wants_counts[person] = 0
        for other in og_people_list:
            if person in og_wants[other]:
                og_wants_counts[person] += 1

    og_strong_pairs: list[tuple[str, str]] = []
    for person in og_people_list:
        for other in og_people_list:
            if other not in og_wants[person]:
                continue
            elif person not in og_wants[other]:
                continue
            pair = (min(other, person), max(other, person))
            if pair not in og_strong_pairs:
                og_strong_pairs.append(pair)
    og_strong_pairs.sort(key=lambda x: og_wants_counts[x[0]] + og_wants_counts[x[1]])

    og_adjacents: dict[str, set[str]] = {}
    for person in og_people_list:
        og_adjacents[person] = set()
    for person in og_people_list:
        for other in og_wants[person]:
            og_adjacents[person].add(other)
            og_adjacents[other].add(person)

    return og_strong_pairs, og_wants_counts, og_adjacents


def gen_rooms(
    og_people_list: list[str],
    og_wants_counts: dict[str, int],
    og_adjacents: dict[str, set[str]],
    og_strong_pairs: list[tuple[str, str]],
):
    all_rooms: list[list[str]] = []
    people_remaining = og_people_list.copy()
    while len(people_remaining) > 0:
        people_in_room: list[str] = []

        for pair in og_strong_pairs:
            if pair[0] in people_remaining and pair[1] in people_remaining:
                people_in_room.append(pair[0])
                people_in_room.append(pair[1])
                print(f"selecting base pair {pair[0]} <-> {pair[1]}")
                break

        if len(people_in_room) == 0:
            print(f"base pair failed. selecting {people_remaining[0]}")
            people_in_room = [people_remaining[0]]

        while len(people_in_room) < 4:
            adjacents: set[str] = set()
            for person in people_in_room:
                adjacents = adjacents.union(og_adjacents[person])

            bad: list[str] = []
            for adj in adjacents:
                if adj in people_in_room:
                    bad.append(adj)
                elif adj not in people_remaining:
                    bad.append(adj)
            for x in bad:
                adjacents.remove(x)

            if len(adjacents) == 0:
                break  # break finding ppl for the room

            min_adjacent: str | None = None
            min_want_count = 100000000000
            for adj in adjacents:
                count = og_wants_counts[adj]
                # count = min(count, len(og_wants[adj]))
                if count < min_want_count:
                    min_adjacent = adj
                    min_want_count = count

            assert min_adjacent is not None
            print(f"adding {min_adjacent} with outer count {min_want_count}.")
            # print(f"from pool: {adjacents}")
            people_in_room.append(min_adjacent)
        # end searching for the room

        all_rooms.append(people_in_room)
        for person in people_in_room:
            people_remaining.remove(person)

        print(f"Adding room: {people_in_room}\n")

    return all_rooms


def count_issues(
    og_people_list: list[str],
    og_wants: dict[str, list[str]],
    all_rooms: list[list[str]],
):
    problem_count: int = 0
    for p in og_people_list:
        person_room = None
        for room in all_rooms:
            if p in room:
                person_room = room

        if person_room is None:
            problem_count += 1
            print(f"{p} didn't get a room!")
            continue

        score: int = 0
        for w in og_wants[p]:
            if w in person_room:
                score += 1

        if score < 1:
            problem_count += 1
            print(f"{p} got {score} matches.")

    for i, r in enumerate(all_rooms):
        if len(r) < 3:
            problem_count += 1
            print(f"room {i} only has {len(r)} people.")
    print(f"{problem_count} problem(s)")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage:\n\t-c file outputted\n\tfile [-o outfile]")

    if sys.argv[1] == "-c":
        ppl, wants = load_things(sys.argv[2])
        rooms: list[list[str]] = []

        with open(sys.argv[3], newline="") as f:
            reader = csv.reader(f)
            for line in reader:
                rooms.append(line)

        count_issues(ppl, wants, rooms)
    else:
        ppl, wants = load_things(sys.argv[1])
        strong, want_counts, adjacents = gen_strong_pairs_wants_counts_and_adjacents(
            ppl, wants
        )
        all_rooms = gen_rooms(ppl, want_counts, adjacents, strong)

        for r in all_rooms:
            print(r)

        count_issues(ppl, wants, all_rooms)

        if len(sys.argv) >= 3:
            out_path = "rooms.csv"
            if len(sys.argv) >= 4:
                out_path = sys.argv[3]

            f = open(out_path, "w")
            for room in all_rooms:
                line = ""
                for person in room:
                    line = line + person + ","
                line = line + "\n"
                f.write(line)
            f.close()
            print(f"outputted rooms written to '{out_path}'")
