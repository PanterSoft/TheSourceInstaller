# Zsh completion script for TSI
# Source this file or add to your fpath

_tsi() {
    local context state line
    typeset -A opt_args

    _arguments -C \
        "1: :->command" \
        "*::arg:->args"

    case $state in
        command)
            local commands=(
                "install:Install a package"
                "remove:Remove an installed package"
                "list:List installed packages"
                "info:Show package information"
                "update:Update package repository"
                "uninstall:Uninstall TSI"
                "--help:Show help"
                "--version:Show version"
            )
            _describe 'command' commands
            ;;
        args)
            case ${words[1]} in
                install)
                    _arguments \
                        "--force[Force reinstall]" \
                        "--prefix[Installation prefix]:directory:_files -/" \
                        "*:package:->packages"
                    ;;
                remove)
                    _arguments \
                        "*:package:->installed_packages"
                    ;;
                info)
                    _arguments \
                        "*:package:->packages"
                    ;;
                update)
                    _arguments \
                        "--repo[Repository URL]:url:_urls" \
                        "--local[Local path]:directory:_files -/" \
                        "--prefix[Installation prefix]:directory:_files -/"
                    ;;
                uninstall)
                    _arguments \
                        "--all[Remove all TSI data]" \
                        "--prefix[Installation prefix]:directory:_files -/"
                    ;;
                list)
                    # List command takes no arguments
                    _arguments
                    ;;
                --help|--version|-h|-v)
                    # These take no arguments
                    _arguments
                    ;;
            esac
            ;;
    esac

    case $words[1] in
        install|info)
            if [[ $state == packages ]]; then
                local repo_dir="${HOME}/.tsi/repos"
                if [ -d "$repo_dir" ]; then
                    local packages=($(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null))
                    if [ ${#packages[@]} -gt 0 ]; then
                        _describe 'package' packages
                    fi
                fi
            fi
            ;;
        remove)
            if [[ $state == installed_packages ]]; then
                local installed=($(tsi list 2>/dev/null | grep -E "^  " | awk '{print $1}' | sed 's/://' 2>/dev/null))
                if [ ${#installed[@]} -gt 0 ]; then
                    _describe 'installed package' installed
                fi
            fi
            ;;
    esac
}

compdef _tsi tsi
