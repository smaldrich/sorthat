import csv
import sys

# FILE IMPORT

assert len(sys.argv) == 2


class Link:
    def __init__(self, p1: str, p2: str):
        self.p1 = min(p1, p2)
        self.p2 = max(p1, p2)
        self.strength = 1

    def __eq__(self, value):
        return self.p1 == value.p1 and self.p2 == value.p2

    def __hash__(self):
        return self.p1.__hash__() + self.p2.__hash__()

    def __str__(self):
        return f"{self.p1} <-> {self.p2}"
        pass


og_links: dict[Link, int] = {}
og_people: list[str] = []
og_wants: dict[str, list[str]] = {}
og_genders: dict[str, str] = {}
og_people_by_least_wants: list[tuple[str, int]] = []
with open(sys.argv[1], newline="") as f:
    reader = csv.reader(f)
    for i, line in enumerate(reader):
        if i == 0:
            continue  # FIXME: should cope with reordering columns in the data
        name: str = line[0].lower().strip()
        gender: str = line[1].lower().strip()
        wants: list[str] = line[2].lower().split(", ")
        wants = [x.strip() for x in wants]

        og_people.append(name)
        og_genders[name] = gender
        og_wants[name] = wants
        for r in wants:
            link = Link(name, r)
            if link in og_links:
                og_links[link] += 1
            else:
                og_links[link] = 1

want_counts: dict[str, int] = {}
for person in og_people:
    count: int = 0
    for other in og_people:
        if person in og_wants[other]:
            count += 1
    want_counts[person] = count

sorted_people = og_people.copy()
sorted_people.sort(key=lambda x: want_counts[x])

# END FILE IMPORT

# CULLING BAD LINKS

bad_links: list[Link] = []
for link in og_links:
    if link.p1 not in og_people or link.p2 not in og_people:
        print(f"link with an invalid name: {link}, removing.")
        bad_links.append(link)
    elif og_genders[link.p1] != og_genders[link.p2]:
        print(f"invalid gender link: {link}, removing.")
        bad_links.append(link)
    elif og_genders[link.p1] != og_genders[link.p2]:
        bad_links.append(link)

for link in bad_links:
    # FIXME: file src
    og_links.pop(link)

# END CULLING BAD LINKS

all_rooms: list[list[str]] = []
while len(og_links) > 0:
    links_in_room = []
    people_in_room = set()
    for link, strength in og_links.items():
        if strength == 2:
            links_in_room.append(link)
            people_in_room.add(link.p1)
            people_in_room.add(link.p2)
            break

    if len(links_in_room) == 0:
        links_in_room.append(og_links.popitem()[0])
        people_in_room.add(link.p1)
        people_in_room.add(link.p2)

    out_of_links = False
    while len(people_in_room) < 4 and not out_of_links:
        for floater in sorted_people:
            if floater in people_in_room:
                continue

            found = None
            for link in og_links:
                if link in links_in_room:
                    continue
                elif link.p1 == floater or link.p2 == floater:
                    if link.p1 in people_in_room or link.p2 in people_in_room:
                        found = link
                        break
            if found is None:
                out_of_links = True
                break

            people_in_room.add(found.p1)
            people_in_room.add(found.p2)
            links_in_room.append(found)
            break

    # remove any links containing ppl in this room before moving on
    bad_links = set()
    for person in people_in_room:
        sorted_people.remove(person)
        for link in og_links:
            if person == link.p1 or person == link.p2:
                bad_links.add(link)
    for link in bad_links:
        og_links.pop(link)

for room in all_rooms:
    print(room)

# PRINTING ISSUES
print("\nIssues:")

for p in og_people:
    person_room = None
    for room in all_rooms:
        if p in room:
            person_room = room

    if person_room is None:
        print(f"{p} didn't get a room!")
        continue

    score: int = 0
    for w in og_wants[p]:
        if w in person_room:
            score += 1

    if score < 1:
        print(f"{p} got {score} matches.")

for i, r in enumerate(all_rooms):
    if len(r) < 3:
        print(f"room {i} only has {len(r)} people.")
# END PRINTING ISSUES

f = open("rooms.csv", "w")
for room in all_rooms:
    line = ""
    for person in room:
        line = line + person + ","
    line = line + "\n"
    f.write(line)

print("outputted rooms written to 'rooms.csv'")
