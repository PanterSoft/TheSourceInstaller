# Bash completion script for TSI
# Source this file or add to /etc/bash_completion.d/

# Only run in bash
if [ -z "$BASH_VERSION" ]; then
    return 0
fi

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
            elif [[ ${cur} == *@ ]]; then
                # User typed package@, show versions
                local pkg_name="${cur%@}"
                local repo_dir="${HOME}/.tsi/packages"
                if [ -d "$repo_dir" ] && [ -n "$pkg_name" ]; then
                    # Parse JSON files to get versions
                    local versions=$(python3 -c "
import json, os, sys
repo_dir = os.path.expanduser('$repo_dir')
pkg_name = '$pkg_name'
versions = []
for f in os.listdir(repo_dir):
    if f.endswith('.json'):
        try:
            with open(os.path.join(repo_dir, f), 'r') as file:
                data = json.load(file)
                if data.get('name') == pkg_name:
                    versions.append(data.get('version', 'latest'))
        except:
            pass
print(' '.join(sorted(set(versions), reverse=True)))
" 2>/dev/null)
                    if [ -n "$versions" ]; then
                        COMPREPLY=($(compgen -W "${versions}" -- ""))
                        # Prefix with package name and @
                        for i in "${!COMPREPLY[@]}"; do
                            COMPREPLY[$i]="${pkg_name}@${COMPREPLY[$i]}"
                        done
                    fi
                fi
            elif [[ ${cur} == *@* ]]; then
                # User typed package@version, complete version part
                local pkg_name="${cur%%@*}"
                local version_part="${cur#*@}"
                local repo_dir="${HOME}/.tsi/packages"
                if [ -d "$repo_dir" ] && [ -n "$pkg_name" ]; then
                    local versions=$(python3 -c "
import json, os, sys
repo_dir = os.path.expanduser('$repo_dir')
pkg_name = '$pkg_name'
versions = []
for f in os.listdir(repo_dir):
    if f.endswith('.json'):
        try:
            with open(os.path.join(repo_dir, f), 'r') as file:
                data = json.load(file)
                if data.get('name') == pkg_name:
                    versions.append(data.get('version', 'latest'))
        except:
            pass
print(' '.join(sorted(set(versions), reverse=True)))
" 2>/dev/null)
                    if [ -n "$versions" ]; then
                        COMPREPLY=($(compgen -W "${versions}" -- "${version_part}"))
                        # Prefix with package name and @
                        for i in "${!COMPREPLY[@]}"; do
                            COMPREPLY[$i]="${pkg_name}@${COMPREPLY[$i]}"
                        done
                    fi
                fi
            else
                # List packages from repository
                local repo_dir="${HOME}/.tsi/packages"
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
            if [[ ${cur} == *@ ]]; then
                # User typed package@, show versions
                local pkg_name="${cur%@}"
                local repo_dir="${HOME}/.tsi/packages"
                if [ -d "$repo_dir" ] && [ -n "$pkg_name" ]; then
                    local versions=$(python3 -c "
import json, os, sys
repo_dir = os.path.expanduser('$repo_dir')
pkg_name = '$pkg_name'
versions = []
for f in os.listdir(repo_dir):
    if f.endswith('.json'):
        try:
            with open(os.path.join(repo_dir, f), 'r') as file:
                data = json.load(file)
                if data.get('name') == pkg_name:
                    versions.append(data.get('version', 'latest'))
        except:
            pass
print(' '.join(sorted(set(versions), reverse=True)))
" 2>/dev/null)
                    if [ -n "$versions" ]; then
                        COMPREPLY=($(compgen -W "${versions}" -- ""))
                        for i in "${!COMPREPLY[@]}"; do
                            COMPREPLY[$i]="${pkg_name}@${COMPREPLY[$i]}"
                        done
                    fi
                fi
            elif [[ ${cur} == *@* ]]; then
                # User typed package@version, complete version part
                local pkg_name="${cur%%@*}"
                local version_part="${cur#*@}"
                local repo_dir="${HOME}/.tsi/packages"
                if [ -d "$repo_dir" ] && [ -n "$pkg_name" ]; then
                    local versions=$(python3 -c "
import json, os, sys
repo_dir = os.path.expanduser('$repo_dir')
pkg_name = '$pkg_name'
versions = []
for f in os.listdir(repo_dir):
    if f.endswith('.json'):
        try:
            with open(os.path.join(repo_dir, f), 'r') as file:
                data = json.load(file)
                if data.get('name') == pkg_name:
                    versions.append(data.get('version', 'latest'))
        except:
            pass
print(' '.join(sorted(set(versions), reverse=True)))
" 2>/dev/null)
                    if [ -n "$versions" ]; then
                        COMPREPLY=($(compgen -W "${versions}" -- "${version_part}"))
                        for i in "${!COMPREPLY[@]}"; do
                            COMPREPLY[$i]="${pkg_name}@${COMPREPLY[$i]}"
                        done
                    fi
                fi
            else
                # List packages from repository
                local repo_dir="${HOME}/.tsi/packages"
                if [ -d "$repo_dir" ]; then
                    local packages=$(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null)
                    if [ -n "$packages" ]; then
                        COMPREPLY=($(compgen -W "${packages}" -- ${cur}))
                    fi
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
                COMPREPLY=($(compgen -W "--prefix" -- ${cur}))
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
                local repo_dir="${HOME}/.tsi/packages"
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
                local repo_dir="${HOME}/.tsi/packages"
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
                COMPREPLY=($(compgen -W "--prefix" -- ${cur}))
                return 0
            fi
        fi
    done

    return 0
}

complete -F _tsi tsi
