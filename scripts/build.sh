while getopts "rd" opt; do
  case $opt in
    r) release=1 ;;
    d) debug=1 ;;
    ?) echo "usage: $0 [-r] [-d]"; exit 1 ;;
  esac
done

if [[ $release -eq 0 && $debug -eq 0 ]]; then
  debug = 1
fi

[[ $debug -eq 1 ]] && cmake --build build/linux-debug
[[ $release -eq 1 ]] &&  cmake --build build/linux-release