#!/bin/bash

# List of commands and libraries to check
commands=("cmake" "make" "buildconf")
libraries=("library1" "library2" "library3")

# Declare an associative array to store availability
declare -A command_availability
declare -A library_availability

# Function to check if a command is available
check_command() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if a library is available
check_library() {
    ldconfig | grep -q "$1"
}

# Check if all commands are available
for cmd in "${commands[@]}"; do
    if check_command "$cmd"; then
        command_availability["$cmd"]="1"
    else
        command_availability["$cmd"]="0"
    fi
done

# Check if all libraries are available
for lib in "${libraries[@]}"; do
    if check_library "$lib"; then
        library_availability["$lib"]="1"
    else
        library_availability["$lib"]="0"
        exit 1
    fi
done

# Print the availability of commands
for command in "${!command_availability[@]}"; do
    echo "Command '$command' is ${command_availability[$command]}"
done

# Print the availability of libraries
for library in "${!library_availability[@]}"; do
    echo "Library '$library' is ${library_availability[$library]}"
done