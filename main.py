import csv
import sys

assert len(sys.argv) == 2

all_wants: dict[str, list[str]] = {}
all_genders: dict[str, str] = {}
all_people: list[str] = []
all_rooms: list[list[str]] = []

with open(sys.argv[1], newline="") as f:
    reader = csv.reader(f)
    for i, line in enumerate(reader):
        if i == 0:
            continue
        name: str = line[0].lower().strip()
        gender: str = line[1].lower().strip()
        roomies: list[str] = line[2].lower().split(", ")
        roomies = [x.strip() for x in roomies]

        all_people.append(name)
        all_genders[name] = gender
        all_wants[name] = roomies

for name in all_people:
    weird = []
    for want in all_wants[name]:
        if want not in all_people:
            print(f"removing unknown want from {name}, '{want}'")
            weird.append(want)

    for w in weird:
        all_wants[name].remove(w)

    if weird.__len__() != 0:
        print(f"fixed of {name}: {all_wants[name]}")


def max_room(rooms, person) -> tuple[list[str], int]:
    maxRoom = None
    maxRoomScore = -100000000
    for room in all_rooms:
        if len(room) == 4:
            continue
        elif len(room) > 0 and all_genders[person] != all_genders[room[0]]:
            continue
        score = -len(room) * 0.5

        # outgoing links
        wanted = all_wants[person]
        for w in wanted:
            if w in room:
                score += 2

        # incoming links
        for other in room:
            otherWanted = all_wants[other]
            if person in otherWanted:
                score += 1

        if score > maxRoomScore:
            maxRoomScore = score
            maxRoom = room

    return maxRoom, maxRoomScore


people_backup = all_people.copy()
while len(all_people) > 0:
    person = all_people.pop()
    room, score = max_room(all_rooms, person)

    if room is None or score < 0:
        all_rooms.append([person])
    else:
        room.append(person)
all_people = people_backup


def room_match_coutns():
    print("DONEZO GARBANZO!!!!!\n")
    for i, r in enumerate(all_rooms):
        print(f"{i}\t{r}")
        matchCount: int = 0
        for person in r:
            wanted = all_wants[person]
            for w in wanted:
                if w in r:
                    matchCount += 1
        print(f"\t{matchCount} matches.")


def issues():
    print("\nIssues:")

    for p in all_people:
        person_room = None
        for room in all_rooms:
            if p in room:
                person_room = room

        if person_room is None:
            print(f"{p} didn't get a room!")
            continue

        score: int = 0
        for w in all_wants[p]:
            if w in person_room:
                score += 1

        if score < 1:
            print(f"{p} got {score} matches.")

    for i, r in enumerate(all_rooms):
        if len(r) < 3:
            print(f"room {i} only has {len(r)} people.")


issues()

f = open("rooms.csv", "w")
for room in all_rooms:
    line = ""
    for person in room:
        line = line + person + ","
    line = line + "\n"
    f.write(line)

print("outputted rooms written to 'rooms.csv'")
