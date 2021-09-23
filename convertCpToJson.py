import json
import sys
import re

def main(argv):
    file_name = str(argv)
    contacts = []
    with open(file_name, 'r') as cf:
        count = 0
        for contact in cf.readlines():
            if contact[0] == '#':
                continue
            if not contact.startswith('a contact'):
                continue

            contact = contact.translate(str.maketrans('', '', '+'))
            contact = re.sub(' +', ' ', contact)
            fields = contact.split(' ')[2:]  # ignore "a contact"
            start, end, frm, to, rate = map(int, fields)
            #owlt (one-way light time) is in ION ranges, not contacts
            contacts.append({"contact": count, "source": frm, "dest": to, \
                             "startTime": start, "endTime": end, "rate": rate, \
                             "owlt": 0})
            count += 1
    __contact_plan = {"contacts": contacts}
    print(__contact_plan)
    with open('cgrTable.json', 'w') as outfile:
        json.dump(__contact_plan, outfile, default=set_default)

def set_default(obj):
    if isinstance(obj, set):
        return list(obj)
    raise TypeError

if __name__ == "__main__":
    main(sys.argv[1])

