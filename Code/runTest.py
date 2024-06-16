import os
import subprocess
import sys

# define a global variable name for compiler
exec_name = "shell.exe"
testing_location = "../Testing"
shell_c_path = "shell.c"

def compile_shell():
    compile_cmd = ["gcc", "-Wall", "-Werror", shell_c_path, "-o", exec_name]
    
    try:
        subprocess.run(compile_cmd, check=True, capture_output=True, text=True)
        print("\033[94mCompilation succeeded\033[0m\n")
    except subprocess.CalledProcessError as e:
        print("\033[91mCompilation failed with the following output:\033[0m")
        print(e.stderr)
        sys.exit(1)


def get_basename(path):
    return 

def run_tests_in_directory(directory):
    print(f"Running tests in directory: {os.path.basename(directory)}")
    for root, _, files in os.walk(directory):
        test_files = sorted([f for f in files if f.endswith('.input')])
        for input_file in test_files:
            test_base = os.path.splitext(input_file)[0]
            expected_output = f"{test_base}.output"
            program_output = f"{test_base}.result"

            with open(os.path.join(root, input_file), 'r') as infile, open(os.path.join(root, program_output), 'w') as outfile:
                subprocess.run([f"./{exec_name}"], stdin=infile, stdout=outfile, text=True)
            
            diff_cmd = ["diff", "-q", "-Z", os.path.join(root, expected_output), os.path.join(root, program_output)]
            result = subprocess.run(diff_cmd, capture_output=True, text=True)

            if result.returncode == 0:
                print(f"\033[92m{input_file} : PASS\033[0m")
            else:
                print(f"\033[91m{input_file}: FAIL\033[0m")

def main():
    compile_shell()
    
    for entry in os.listdir(testing_location):
        full_path = os.path.join(testing_location, entry)
        if os.path.isdir(full_path):
            run_tests_in_directory(full_path)

if __name__ == "__main__":
    main()
