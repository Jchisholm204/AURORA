file(REMOVE_RECURSE
  "libbarf.pdb"
  "libbarf.so"
)

# Per-language clean rules from dependency scanning.
foreach(lang C)
  include(CMakeFiles/barf.dir/cmake_clean_${lang}.cmake OPTIONAL)
endforeach()
