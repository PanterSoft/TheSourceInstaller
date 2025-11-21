# Bash completion script for TSI
# Source this file or add to /etc/bash_completion.d/

_tsi() {
    local cur prev words cword
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    words=("${COMP_WORDS[@]}")
    cword=$COMP_CWORD

    # All TSI commands
    local commands="install remove list info update uninstall --help --version -h -v"

    # Handle command-specific completions
    case ${prev} in
        install)
            if [[ ${cur} == -* ]]; then
                COMPREPLY=($(compgen -W "--force --prefix" -- ${cur}))
            else
                # List packages from repository
                local repo_dir="${HOME}/.tsi/repos"
                if [ -d "$repo_dir" ]; then
                    local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
                    if [ -n "$packages" ]; then
                        COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
                    fi
                fi
            fi
            return 0
            ;;

        remove)
            # List installed packages
            local installed=$(tsi list 2>/dev/null | grep -E "^  " | awk '{print $1}' | sed 's/://' 2>/dev/null)
            if [ -n "$installed" ]; then
                COMPREPLY=($(compgen -W "${installed}" -- ${cur}))
            fi
            return 0
            ;;

        info)
            # List packages from repository
            local repo_dir="${HOME}/.tsi/repos"
            if [ -d "$repo_dir" ]; then
                local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
                if [ -n "$packages" ]; then
                    COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
                fi
            fi
            return 0
            ;;

        update)
            if [[ ${cur} == -* ]]; then
                COMPREPLY=($(compgen -W "--repo --local --prefix" -- ${cur}))
            fi
            return 0
            ;;

        uninstall)
            if [[ ${cur} == -* ]]; then
                COMPREPLY=($(compgen -W "--all --prefix" -- ${cur}))
            fi
            return 0
            ;;

        --prefix|--local)
            # Complete directories
            COMPREPLY=($(compgen -d -- ${cur}))
            return 0
            ;;

        --repo)
            # Could complete URLs, but that's complex - just return
            return 0
            ;;
    esac

    # Handle options that need arguments
    if [[ ${prev} == "--prefix" ]] || [[ ${prev} == "--local" ]]; then
        COMPREPLY=($(compgen -d -- ${cur}))
        return 0
    fi

    # First word - complete commands
    if [[ ${cword} -eq 1 ]]; then
        COMPREPLY=($(compgen -W "${commands}" -- ${cur}))
        return 0
    fi

    # Handle multi-word commands (e.g., "install --force <package>")
    # Check if we're in an install command context
    for ((i=1; i < ${#COMP_WORDS[@]}; i++)); do
        if [[ "${COMP_WORDS[i]}" == "install" ]]; then
            # We're in install command
            if [[ ${prev} == "--force" ]] || [[ ${prev} == "--prefix" ]]; then
                # After --force or --prefix, complete packages
                local repo_dir="${HOME}/.tsi/repos"
                if [ -d "$repo_dir" ]; then
                    local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
                    if [ -n "$packages" ]; then
                        COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
                    fi
                fi
                return 0
            elif [[ ${cur} == -* ]]; then
                # Complete install options
                COMPREPLY=($(compgen -W "--force --prefix" -- ${cur}))
                return 0
            else
                # Complete packages
                local repo_dir="${HOME}/.tsi/repos"
                if [ -d "$repo_dir" ]; then
                    local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
                    if [ -n "$packages" ]; then
                        COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
                    fi
                fi
                return 0
            fi
        elif [[ "${COMP_WORDS[i]}" == "update" ]]; then
            # We're in update command
            if [[ ${prev} == "--repo" ]] || [[ ${prev} == "--local" ]] || [[ ${prev} == "--prefix" ]]; then
                # After these options, no more completion needed
                return 0
            elif [[ ${cur} == -* ]]; then
                COMPREPLY=($(compgen -W "--repo --local --prefix" -- ${cur}))
                return 0
            fi
        elif [[ "${COMP_WORDS[i]}" == "uninstall" ]]; then
            # We're in uninstall command
            if [[ ${prev} == "--prefix" ]]; then
                COMPREPLY=($(compgen -d -- ${cur}))
                return 0
            elif [[ ${cur} == -* ]]; then
                COMPREPLY=($(compgen -W "--all --prefix" -- ${cur}))
                return 0
            fi
        fi
    done

    return 0
}

complete -F _tsi tsi
