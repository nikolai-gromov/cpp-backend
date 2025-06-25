# Multiplayer Game "Dog Story"

## Project Description

This is a multiplayer game with a C++ backend where players control a dog that delivers lost items to a lost and found office. The goal of the game is to score as many points as possible. The gameplay is simple: a map is selected, and the player controls a character—a dog—that finds lost items and brings them to the office.

### Game Architecture

The game has a client-server architecture:

- **Server**: Manages the game, processes commands, and provides a REST API.
- **Client**: Accepts user input, sends requests to the server, and displays the current state of the game.

The project is built using CMake and containerized with Docker. The project is currently under development and is not finished.

## Technology Stack

- **Programming Language**: C++
- **Compiler**: GCC with C++20 support
- **Package Manager**: Conan
- **Build System**: CMake
- **Containerization**: Docker
- **Libraries**:
    - **STL**: Standard Template Library for C++.
    - **Boost**: A set of libraries that extend the functionality of C++.
- **Testing Framework**:
    - **Catch2**: A lightweight framework for writing tests in C++.

## Installation and Running

### 1. Install Dependencies on Ubuntu

Open a terminal and connect to your server via SSH, then execute the following commands:

```bash
sudo apt update
sudo apt upgrade -y

# install necessary development tools for Python and C/C++, as well as version control and build automation systems
sudo apt install python3-pip build-essential gcc-10 g++-10 cpp-10 git cmake -y

# set GCC-10 as the default version
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 10

# install conan version 1
sudo pip3 install conan==1.*
```

### 2. Install Docker on Ubuntu

The easiest way to install Docker on Linux is to download and run the official script.

1. **Install the utility for downloading files**:
    ```bash
    sudo apt install curl
    ```

2. **Download the Docker installation script**:
    ```bash
    curl -fsSL https://get.docker.com -o get-docker.sh
    ```

3. **Run the script**:
    ```bash
    sh get-docker.sh
    ```

If you encounter errors while executing these commands, install Docker manually. Installation instructions can be found in [the official Docker documentation](https://docs.docker.com/engine/install/ubuntu/).

### 3. Clone the Repository

```bash
git clone git@github.com:nikolai-gromov/cpp-backend.git
cd cpp-backend/backend
```

### 4. Build the Project with CMake

#### Build in Debug Mode

1. **Create a build directory**:
    ```bash
    mkdir build
    cd build
    ```

2. **Install dependencies using Conan**:
    ```bash
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Debug
    ```

3. **Configure the project with CMake**:
    ```bash
    cmake -DCMAKE_BUILD_TYPE=Debug ..
    ```

4. **Build the project**:
    ```bash
    cmake --build .
    ```

#### Build in Release Mode

1. **Create a build directory**:
    ```bash
    mkdir build
    cd build
    ```

2. **Install dependencies using Conan**:
    ```bash
    conan install .. -s compiler.libcxx=libstdc++11 -s build_type=Release
    ```

3. **Configure the project with CMake**:
    ```bash
    cmake -DCMAKE_BUILD_TYPE=Release ..
    ```

4. **Build the project**:
    ```bash
    cmake --build .
    ```

### 5. Run Using Docker

```bash
docker build -t game_http_server .
docker run --rm -p 80:8080 game_http_server
```

To load the page, enter in your browser's address bar: http://<IP-address or domain>.

## Backend Description

The server side of the game is written in C++ and performs the following functions:

- Command line processing.
- Serving static files.
- Player authentication.
- Game state management.
- Handling commands for character control.
- Collecting and delivering items.
- Updating time on external request.
- Automatic game state updates.
- Saving game state.
- Unit testing.

### Future Plans

The project will continue to evolve. Future plans include:

- Releasing characters if players are inactive.
- Removing tokens of inactive players.
- Creating a character data storage system.
- Organizing record storage in PostgreSQL.

## Contribution

If you would like to contribute to the project, please create a fork of the repository and submit a pull request with your changes.

## Contact

For questions and suggestions, please contact me at em.dev.nik@gmail.com.