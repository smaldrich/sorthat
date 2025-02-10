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


all_links: dict[Link, int] = {}
all_people: list[str] = []
all_genders: dict[str, str] = {}
with open(sys.argv[1], newline="") as f:
    reader = csv.reader(f)
    for i, line in enumerate(reader):
        if i == 0:
            continue  # FIXME: should cope with reordering columns in the data
        name: str = line[0].lower().strip()
        gender: str = line[1].lower().strip()
        roomies: list[str] = line[2].lower().split(", ")
        roomies = [x.strip() for x in roomies]

        all_people.append(name)
        all_genders[name] = gender
        for r in roomies:
            link = Link(name, r)
            if link in all_links:
                all_links[link] += 1
            else:
                all_links[link] = 0

# END FILE IMPORT

# CULLING BAD LINKS

bad_links = []
for link in all_links:
    if link[0] not in all_people or link[1] not in all_people:
        bad_links.append(link)

for link in bad_links:
    # FIXME: file src
    print(f"bad link: {link}, removing.")
    all_links.remove(link)

# END CULLING BAD LINKS

all_rooms: list[list[str]] = [[] for i in range(9)]

people_in_room = []
for link, strength in all_links:
    people_in_room.append(link.p1)

all_rooms.append(people_in_room)
