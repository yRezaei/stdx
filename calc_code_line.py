import os
import sys

def count_lines_in_file(file_path):
    """Count the number of lines in a single file."""
    try:
        with open(file_path, 'r') as file:
            return sum(1 for line in file)
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        return 0

def count_lines_in_directory(directory):
    """Count the total number of lines in all files within a directory and its subdirectories."""
    total_lines = 0
    for root, dirs, files in os.walk(directory):
        for file in files:
            file_path = os.path.join(root, file)
            total_lines += count_lines_in_file(file_path)
    return total_lines

if __name__ == "__main__":
    # Check if a path is provided as a command-line argument
    if len(sys.argv) != 2:
        print("Usage: python script.py <path>")
        sys.exit(1)
    
    path = sys.argv[1]
    
    # Handle the case where the path is a file
    if os.path.isfile(path):
        total_lines = count_lines_in_file(path)
        print(f"Total lines in {path}: {total_lines}")
    
    # Handle the case where the path is a directory
    elif os.path.isdir(path):
        total_lines = count_lines_in_directory(path)
        print(f"Total lines in {path}: {total_lines}")
    
    # Handle invalid paths
    else:
        print(f"{path} is not a valid file or directory")
        sys.exit(1)