# Bash completion script for TSI
# Source this file or add to /etc/bash_completion.d/

_tsi() {
    local cur prev words cword
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    words=("${COMP_WORDS[@]}")
    cword=$COMP_CWORD

    # Commands
    local commands="install remove uninstall list info update --help --version -h -v"

    # Options for install command
    if [[ ${prev} == "install" ]]; then
        if [[ ${cur} == -* ]]; then
            COMPREPLY=($(compgen -W "--force --prefix" -- ${cur}))
        else
            # List packages from repository
            local repo_dir="${HOME}/.tsi/repos"
            if [ -d "$repo_dir" ]; then
                local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
                COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
            fi
        fi
        return 0
    fi

    # Options for remove/uninstall command
    if [[ ${prev} == "remove" ]] || [[ ${prev} == "uninstall" ]]; then
        # List installed packages
        local installed=$(tsi list 2>/dev/null | grep -E "^  " | awk '{print $1}' | sed 's/://' 2>/dev/null)
        if [ -n "$installed" ]; then
            COMPREPLY=($(compgen -W "${installed}" -- ${cur}))
        fi
        return 0
    fi

    # Options for info command
    if [[ ${prev} == "info" ]]; then
        # List packages from repository
        local repo_dir="${HOME}/.tsi/repos"
        if [ -d "$repo_dir" ]; then
            local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
            COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
        fi
        return 0
    fi

    # Options for update command
    if [[ ${prev} == "update" ]]; then
        if [[ ${cur} == -* ]]; then
            COMPREPLY=($(compgen -W "--repo --local --prefix" -- ${cur}))
        fi
        return 0
    fi

    # Handle --prefix option
    if [[ ${prev} == "--prefix" ]]; then
        # Complete directories
        COMPREPLY=($(compgen -d -- ${cur}))
        return 0
    fi

    # Handle --local option
    if [[ ${prev} == "--local" ]]; then
        # Complete directories
        COMPREPLY=($(compgen -d -- ${cur}))
        return 0
    fi

    # First word - complete commands
    if [[ ${cword} -eq 1 ]]; then
        COMPREPLY=($(compgen -W "${commands}" -- ${cur}))
        return 0
    fi

    return 0
}

complete -F _tsi tsi

