# Archive Update Utility

This utility is designed to update the modification time of files within an archive.

## Usage

To run the utility, execute the following command:

```./archive_update <archive_path> <file_type>```

- `<archive_path>`: The path to the archive file.
- `<file_type>`: The type of files to update within the archive.

## Functionality

The utility performs the following steps:

1. Creates a temporary directory.
2. Extracts the contents of the archive to the temporary directory.
3. Calculates the maximum modification time of files matching the specified file type within the temporary directory.
4. Updates the modification time of all files in the temporary directory to the maximum modification time.
5. Creates a temporary archive with the updated files.
6. Removes the old archive.
7. Renames the temporary archive to the original archive name.
8. Removes the temporary directory.

## Example

Here is an example command to update the modification time of all `.txt` files within the `archive.tar` archive:

```./archive_update archive.tar txt```

## Note

- This utility relies on the `tar` command-line tool to extract and create archives. Please ensure that `tar` is installed and accessible in your system's PATH.
- The utility handles regular files (`DT_REG`) within the archive and ignores directories and other file types.
