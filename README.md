# Modem AT Chat

This project provides an easy and straightforward way to communicate with a modem device using AT commands. The program reads AT commands and their expected results from a designated file, sends the commands to the modem, and automatically verifies the received responses against the predefined expected outcomes.

## Features

* Write AT commands and expected results in a file.
* Automaticly send AT commands from specify file to modem device.
* Comparison of modem responses with predefined expected results.

## Getting Started

### Prerequisites

* A modem device connected to your computer with the appropriate serial port (e.g., /dev/ttyUSB0 on Linux).

* C/C++ development environment with a working compiler (gcc and cmake).

### Installation

1. Clone the repository:
```
git clone https://github.com/Mark-Walen/modem-at-chat/tree/main
```
2. Build the executable:

```shell
./build.sh
```

## Usage
1. Prepare a file at_commands.txt with the AT commands and their corresponding expected results in the following format:

```
# expect-result | at commands
"" AT\r\n
OK ATZ\r\n
OK # If no more AT commands, just specify last at commands expected result.
```

2. Open the terminal.

3. Run the executable with the file as an argument:

```shell
chat -p /serial/port -f /path/to/chat-file
```

4. The program will read the AT commands and expected responses from the file, send the commands to the modem, and compare the received responses with the predefined expected outcomes.

5. If modem response does not match predefined expected results, program will exit.

## Contributing
Contributions to this project are welcome! If you find any issues or have improvements to suggest, please feel free to submit pull requests or open issues on the GitHub repository.

## Contact
If you have any questions or need further assistance, feel free to contact me at walen.mark2509758@gmail.com