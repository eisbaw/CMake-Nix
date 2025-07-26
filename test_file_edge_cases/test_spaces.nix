with import <nixpkgs> {};

stdenv.mkDerivation {
  name = "test-spaces";
  src = ./.;
  
  buildPhase = ''
    # Debug: print what we're working with
    echo "Source parameter: dir with spaces/file.cpp"
    sourceFile="dir with spaces/file.cpp"
    
    echo "Looking for: $sourceFile"
    
    # List files to debug
    echo "Files in current directory:"
    ls -la
    
    echo "Files in 'dir with spaces':"
    ls -la "dir with spaces" || echo "Directory not found"
    
    # Try to find the file
    if [[ -f "$sourceFile" ]]; then
      echo "Found file at: $sourceFile"
    elif [[ -f "test_file_edge_cases/$sourceFile" ]]; then
      echo "Found file at: test_file_edge_cases/$sourceFile"
    else
      echo "File not found!"
    fi
    
    # The actual compilation would be:
    # gcc -c "$sourceFile" -o test.o
    
    touch $out
  '';
  
  installPhase = "true";
}