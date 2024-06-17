import os
import subprocess
import sys

# Define a global variable name for compiler
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

def run_tests_in_directory(directory):
    print(f"Running tests in directory: {os.path.basename(directory)}")
    for root, _, files in os.walk(directory):
        test_files = sorted([f for f in files if f.endswith('.input')])
        for input_file in test_files:
            test_base = os.path.splitext(input_file)[0]
            expected_output = f"{test_base}.output"
            program_output = f"{test_base}.result"

            input_path = os.path.join(root, input_file)
            output_path = os.path.join(root, program_output)

            with open(input_path, 'r') as infile:
                input_lines = infile.readlines()

            # Check the last line for "exit" or "exit x"
            last_line = input_lines[-1].strip()
            expected_exit_status = None
            if last_line == "exit" or last_line == "status":
                expected_exit_status = 0
            elif last_line.startswith("exit "):
                try:
                    expected_exit_status = int(last_line.split()[1])
                except ValueError:
                    expected_exit_status = None

            # Run the shell program and capture the exit status
            with open(input_path, 'r') as infile, open(output_path, 'w') as outfile:
                process = subprocess.run([f"./{exec_name}"], stdin=infile, stdout=outfile, stderr=subprocess.PIPE, text=True)
                actual_exit_status = process.returncode

                # Append exit status if applicable
                if expected_exit_status is not None:
                    outfile.write(f"Exit status: {actual_exit_status}\n")

            diff_cmd = ["diff", "-q", "-Z", os.path.join(root, expected_output), output_path]
            result = subprocess.run(diff_cmd, capture_output=True, text=True)

            if result.returncode == 0:
                print(f"\033[92m{input_file} : PASS\033[0m")
            else:
                print(f"\033[91m{input_file}: FAIL\033[0m")

def cleanup_temp_files():
    for entry in os.listdir("."):
        if entry != "shell.c" and entry != "runTest.py" and entry != exec_name:
            if os.path.isdir(entry):
                for root, _, files in os.walk(entry):
                    for file in files:
                        os.remove(os.path.join(root, file))
                    os.rmdir(root)
            else:
                os.remove(entry)

def main():
    compile_shell()
    
    for entry in os.listdir(testing_location):
        full_path = os.path.join(testing_location, entry)
        if os.path.isdir(full_path):
            run_tests_in_directory(full_path)
            cleanup_temp_files()

if __name__ == "__main__":
    main()