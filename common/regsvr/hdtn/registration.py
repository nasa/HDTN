import logging
import zmq

class RegistrationManager(object):
    def __init__(self):
        self.registration = {}
        self.command = {
            'DEREGISTER': self.remove,
            'QUERY': self.query,
            'REGISTER': self.add
        }

    def dispatch(self, message):
        msgstr = memoryview(message.buffer).tobytes().decode('utf-8')
        logging.info("Processing registration for " + message.get("Identity") + " at " + message.get("Peer-Address"))
        msgarray = str(msgstr).split(' ')
        if len(msgarray) < 2:
            logging.warn("Invalid command string: " + str(msgarray))
            return False
        command = msgarray[1]
        if command not in self.command:
            logging.warn("Unknown command: `" + str(command) + "`")
            logging.warn(str(msgarray))
            return False
        return self.command[command](message.get('Peer-Address'), message.get('Identity'), msgstr)

    def query(self, origin, identity, msgstr):
        msgarray = msgstr.split(' ')
        if len(msgarray) == 3:
            logging.info("Fetching list for type " + str(msgarray[2]))
            td = {key:value for (key,value) in self.registration.items() if value['type'] == msgarray[2]}
            return list(td.values())
        else:
            logging.info("Fetching full listing ...")
            return list(self.registration.values())

    def add(self, origin, identity, msgstr):
        val = identity.split(':')
        if len(val) >= 3:
            key = origin + ":" + val[1]
            self.registration[key] = {'protocol': 'tcp', 'address': origin, 'type': val[0], 'port': val[1], 'mode': val[2]}
            logging.info(self.registration[key])
            return True
        return None

    def remove(self, origin, identity, msgstr):
        val = identity.split(':')
        if len(val) >= 3:
            key = origin + ":" + val[1]
            if key in self.registration:
                logging.info("Deregistering: " + str(key))
                del self.registration[key]
            else:
                logging.warn("Ignoring deregistration request for unknown service: " + str(key))
            return True
        return None
