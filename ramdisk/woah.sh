#!/mnt/ramdisk/bin/bash

echo "Woah, bash is running on Xanadu!"
 
# This is pure bash, so we don't have cat
# We can, however, print the contents of /mnt/ramdisk/logo.txt as such:
echo "Here's the logo:"

# We can't use cat, but we can use bash's built-in file reading
while IFS= read -r line; do
    echo "$line"
done < /mnt/ramdisk/logo.txt