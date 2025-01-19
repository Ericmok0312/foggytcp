import os

def generate_text_file(filename, size_in_bytes):
    with open(filename, 'w') as f:
        f.write('1' * size_in_bytes)


sizes = [1024, 5*1024, 25*1024, 100*1024, 1024*1024, 10*1024*1024]
for i, size in enumerate(sizes):
    filename = f'foggytcp/testfile/file_{i+1}.txt'
    generate_text_file(filename, size)
    print(f"File '{filename}' with size {size} bytes has been created.")