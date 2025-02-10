import csv
import sys

# FILE IMPORT

assert len(sys.argv) == 2


class Link:
    def __init__(self, p1: str, p2: str):
        self.p1 = p1
        self.p2 = p2

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

for p in og_people:
    wants: int = 0
    for other in og_people:
        if p in og_wants[other]:
            wants += 1
    og_people_by_least_wants.append((p, wants))

og_people_by_least_wants.sort(key=lambda x: x[1])

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

for link in bad_links:
    # FIXME: file src
    og_links.pop(link)

# END CULLING BAD LINKS

# INITIAL BATCHING INTO ROOMS

all_rooms: list[list[str]] = []

while len(og_links) > 0:
    links_in_room = []
    people_in_room = set()
    while len(people_in_room) < 4 and len(og_links) > 0:
        best_link = None
        best_link_score = 0
        for link, value in og_links.items():
            score = 0
            if link.p1 in people_in_room or link.p2 in people_in_room:
                score += 1
            if value == 2:
                score += 10

            if score > best_link_score:
                best_link_score = score
                best_link = link

        if best_link is None:
            best_link = og_links.popitem()[0]
        else:
            og_links.pop(best_link)

        links_in_room.append(best_link)
        people_in_room.add(best_link.p1)
        people_in_room.add(best_link.p2)
    # end selecting links for the room

    # remove any links containing ppl in this room before moving on
    bad_links = set()
    for person in people_in_room:
        for link in og_links:
            if link.p1 == person or link.p2 == person:
                bad_links.add(link)
    for link in bad_links:
        og_links.pop(link)

    print(f"room: {people_in_room}")
    all_rooms.append(people_in_room)

# END INITIAL BATCHING INTO ROOMS

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
