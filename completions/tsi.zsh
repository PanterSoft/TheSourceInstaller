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
                local cur="${words[CURRENT]}"
                if [[ $cur == *@ ]]; then
                    # User typed package@, show versions
                    local pkg_name="${cur%@}"
                    local repo_dir="${HOME}/.tsi/repos"
                    if [ -d "$repo_dir" ] && [ -n "$pkg_name" ]; then
                        local versions=($(python3 -c "
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
" 2>/dev/null))
                        if [ ${#versions[@]} -gt 0 ]; then
                            local version_completions=()
                            for v in "${versions[@]}"; do
                                version_completions+=("${pkg_name}@${v}")
                            done
                            _describe 'version' version_completions
                        fi
                    fi
                elif [[ $cur == *@* ]]; then
                    # User typed package@version, complete version part
                    local pkg_name="${cur%%@*}"
                    local version_part="${cur#*@}"
                    local repo_dir="${HOME}/.tsi/repos"
                    if [ -d "$repo_dir" ] && [ -n "$pkg_name" ]; then
                        local versions=($(python3 -c "
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
" 2>/dev/null))
                        if [ ${#versions[@]} -gt 0 ]; then
                            local version_completions=()
                            for v in "${versions[@]}"; do
                                if [[ "$v" == "$version_part"* ]]; then
                                    version_completions+=("${pkg_name}@${v}")
                                fi
                            done
                            if [ ${#version_completions[@]} -gt 0 ]; then
                                _describe 'version' version_completions
                            fi
                        fi
                    fi
                else
                    # List packages from repository
                    local repo_dir="${HOME}/.tsi/repos"
                    if [ -d "$repo_dir" ]; then
                        local packages=($(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null))
                        if [ ${#packages[@]} -gt 0 ]; then
                            _describe 'package' packages
                        fi
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
