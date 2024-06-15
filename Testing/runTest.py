import os
import subprocess
import filecmp
import difflib
import sys

# define a global variable name for compiler
exec_name = "shell.exe"

def compile_shell():
    shell_c_path = os.path.join("..", "Code", "shell.c")
    compile_cmd = ["gcc", "-Wall", "-Werror", shell_c_path, "-o", exec_name]
    
    try:
        subprocess.run(compile_cmd, check=True, capture_output=True, text=True)
        print("Compilation succeeded.")
    except subprocess.CalledProcessError as e:
        print("Compilation failed with the following output:")
        print(e.stderr)
        sys.exit(1)

def run_tests_in_directory(directory):
    print(f"Running tests in directory: {directory}")
    for root, _, files in os.walk(directory):
        test_files = sorted([f for f in files if f.endswith('.in')])
        for input in test_files:
            test_base = os.path.splitext(input)[0]
            expected_output = f"{test_base}.out"
            program_output = f"{test_base}.res"

            with open(os.path.join(root, input), 'r') as infile, open(os.path.join(root, program_output), 'w') as outfile:
                subprocess.run([f"./{exec_name}"], stdin=infile, stdout=outfile, text=True)
            
            diff_cmd = ["diff", "-q", "-Z", os.path.join(root, expected_output), os.path.join(root, program_output)]
            result = subprocess.run(diff_cmd, capture_output=True, text=True)

            if result.returncode == 0:
                print(f"\033[92m{input} : PASS\033[0m")
            else:
                print(f"\033[91m{input}: FAIL\033[0m")

def main():
    compile_shell()
    
    current_directory = os.getcwd()
    for entry in os.listdir(current_directory):
        if os.path.isdir(entry):
            run_tests_in_directory(entry)

if __name__ == "__main__":
    main()
