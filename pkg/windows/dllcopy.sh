#! /bin/sh

set -e

if [ $# -lt 7 ]; then
  echo Wrong number of arguments
  exit 1
fi

destdir="$1"
strip="$2"
objdump="$3"
cxx="$4"
searchpath="$5"
ldflags="$6"

shift 6

searchpath=`echo $searchpath | sed "s/\\(^\\|:\\)\\/c\\/windows[/a-z0-9]*//gi"`

if [ "$ldflags" != "" ]; then
  for flag in $ldflags; do
    if echo $flag | grep '^-L/' > /dev/null 2>&1; then
      searchpath="$searchpath:${flag#-L*}"
    fi
  done
fi

searchpath="$searchpath:`$cxx -print-search-dirs | sed -n 's/libraries: =//p'`"
searchpath="$searchpath:`$cxx -print-search-dirs | sed -n 's/programs: =//p'`"

#echo "Searchpath: $searchpath"

process_dll()
{
  local depender="$1"
  local dependencies="$2"
  local dll="$3"
  local processed="$destdir/.$dll.processed"
      
  if [ ! -f "$processed" ]; then
    echo "Looking for dependency '$dll' of '$depender'"

    (
      local found_path=
      IFS=':'
            
      for path in $searchpath; do
        if [ -f "$path/.libs/$dll" ]; then
          found_path="$path/.libs/$dll"
          break;
        fi
        
        if [ -f "$path/$dll" ]; then
          found_path="$path/$dll"
          break;
        fi
      done
      
      unset IFS
                  
      echo "processed" > "$processed"
            
      if [ -n "$found_path" ]; then
        echo "Found $dll" as "'$found_path'"
    
        cp -p "$found_path" "$destdir/$dll"
        $strip "$destdir/$dll"
        process_file "$destdir/$dll"                              
      fi
    )        
    
  fi
  
  if [ -f "$destdir/$dll" ]; then
    if [ -f "$destdir/.$dll.dependencies" ]; then  
        cat "$destdir/.$dll.dependencies" >> "$dependencies"
    fi
    echo "$dll" >> "$dependencies"
  fi
  
}

process_dlls()
{
  local depender="$1"
  shift

  depender=$(basename "$depender")
  local dependencies="$destdir/.$depender.dependencies"
  
  while [ ! -z "$1" ]; do
    process_dll "$depender" "$dependencies" "$1"
    echo "$depender" >> "$dependencies"
    sort -u "$dependencies" -o "$dependencies"
    shift
  done
}

process_file()
{
  process_dlls "$1" `$objdump -j .idata -x "$1" | grep 'DLL Name:' | sed 's/ *DLL Name: *//'`
}

process_files()
{  
  rm -f "$destdir"/.*.processed "$destdir"/.*.dependencies 2>/dev/null

  while [ ! -z "$1" ]; do
    process_file "$1"
    shift
  done
}

process_files "$@"

