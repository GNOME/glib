
# Check for bash
[ -z "$BASH_VERSION" ] && return

####################################################################################################

__gsettings() {
  local choices

  case "${COMP_CWORD}" in
    1)
      choices=$'help \nlist-schemas\nlist-relocatable-schemas\nlist-keys \nlist-children \nlist-recursively \nget \nrange \nset \nreset \nwritable \nmonitor'
      ;;

    2)
      case "${COMP_WORDS[1]}" in
        help)
          choices=$'list-schemas\nlist-relocatable-schemas\nlist-keys\nlist-children\nlist-recursively\nget\nrange\nset\nreset\nwritable\nmonitor'
          ;;
        list-keys|list-children|list-recursively)
          choices="$(gsettings list-schemas)"$'\n'"$(gsettings list-relocatable-schemas | sed -e 's.$.:/.')"
          ;;

        get|range|set|reset|writable|monitor)
          choices="$(gsettings list-schemas | sed -e 's.$. .')"$'\n'"$(gsettings list-relocatable-schemas | sed -e 's.$.:/.')"
          ;;
      esac
      ;;

    3)
      case "${COMP_WORDS[1]}" in
        set)
          choices="$(gsettings list-keys ${COMP_WORDS[2]} 2> /dev/null | sed -e 's.$. .')"
          ;;

        get|range|reset|writable|monitor)
          choices="$(gsettings list-keys ${COMP_WORDS[2]} 2> /dev/null)"
          ;;
      esac
      ;;

    4)
      case "${COMP_WORDS[1]}" in
        set)
          range=($(gsettings range ${COMP_WORDS[2]} ${COMP_WORDS[3]} 2> /dev/null))
          case "${range[0]}" in
            enum)
              unset range[0]
             ;;
            *)
              unset range
             ;;
          esac
          local IFS=$'\n'
          choices="${range[*]}"
          ;;
      esac
      ;;
  esac

  local IFS=$'\n'
  COMPREPLY=($(compgen -W "${choices}" "${COMP_WORDS[$COMP_CWORD]}"))
}

####################################################################################################

complete -o nospace -F __gsettings gsettings
