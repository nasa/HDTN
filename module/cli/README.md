## HDTN CLI ##

The HDTN CLI is a command line interface for the HDTN software. It is used to configure and control and instance of HDTN.

### Usage ###

The HDTN CLI is invoked by running the `hdtn-cli` command. HDTN must be running for the CLI to work. Currently, it only supports a limited set of command options. To see the list of options, run `hdtn-cli --help`. To get started:

1. Build HDTN (see instructions in the main README.md)
2. Install HDTN (see instructions in the main README.md)
3. Run an instance of HDTN (see instructions in the main README.md)
4. Run `hdtn-cli --help` for a list of commands
5. Run `hdtn-cli <command1> <command2> ...` to run the desired CLI commands

### Examples ###

The following examples show how to use the CLI to configure and control HDTN.

#### Get a list of available options:
```
$ hdtn-cli --help
Options:
  --help                      Produce help message
  --hostname arg (=127.0.0.1) HDTN hostname
  --port arg (=10305)         HDTN port
  --contact-plan-file arg     Local contact plan file
  --contact-plan-json arg     Contact plan json string
  --outduct[0].rateBps arg    Outduct rate limit (bits per second)
  --outduct[1].rateBps arg    Outduct rate limit (bits per second)
  ```

#### Upload a new contact plan file:
```
$ hdtn-cli --contact-plan-file=/home/user/contact_plan.json
```

#### Upload a new contact plan via json
```
$ hdtn-cli --contact-plan-json='{"contacts":[{"contact":0,"source":102,"dest":10,"startTime":0,"endTime":2000,"rateBitsPerSec":1000000000,"owlt":1},{"contact":1,"source":10,"dest":2,"startTime":0,"endTime":2000,"rateBitsPerSec":1000000000,"owlt":1},	{"contact":2,"source":101,"dest":10,"startTime":0,"endTime":2000,"rateBitsPerSec":1000000000,"owlt":1},{"contact":3,"source":10,"dest":1,"startTime":0,"endTime":2000,"rateBitsPerSec":1000000000,"owlt":1}]}'
```

#### Set the rate limit of the first outduct to 1 Mbps:
```
$ hdtn-cli --outduct[0].rateBps=1000000
```

#### Set the rate limit of the first and second outducts to 1 Mbps:
```
$ hdtn-cli --outduct[0].rateBps=1000000 --outduct[1].rateBps=1000000
```
