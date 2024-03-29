def generate_ansi256_file(filename):
    with open(filename, 'w') as file:
        for i in range(256):
            # ANSI 256 color escape code format: \x1b[48;5;<color_code>m
            ansi_code = f"\x1b[38;5;{i}m"
            # Write ANSI code and corresponding number in text followed by a space
            file.write(ansi_code + f"{i:3}" + ' ')
            if i % 24 == 23:
                # Write a newline character after every 16 colors
                file.write('\n')

# Generate the file
filename = "ansi256_colors.txt"
generate_ansi256_file(filename)
print(f"File '{filename}' generated successfully.")
