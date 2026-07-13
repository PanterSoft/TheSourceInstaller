# Zsh completion script for TSI
# Source this file or add to your fpath

_tsi_versions() {
    local repo_dir="$1" pkg_name="$2"
    [[ -z "$repo_dir" || -z "$pkg_name" ]] && return
    local versions=()
    for f in "$repo_dir"/*.json; do
        [[ -f "$f" ]] || continue
        grep -q "\"name\"[[:space:]]*:[[:space:]]*\"${pkg_name}\"" "$f" 2>/dev/null || continue
        local v
        v=$(grep -o '"version"[[:space:]]*:[[:space:]]*"[^"]*"' "$f" 2>/dev/null | head -1 | sed 's/.*"\([^"]*\)".*/\1/')
        [[ -n "$v" ]] && versions+=("$v")
    done
    (( ${#versions[@]} > 0 )) && echo "${versions[@]}"
}

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
                "uninstall:Remove an installed package"
                "upgrade:Upgrade installed packages"
                "list:List installed packages"
                "search:Search available packages"
                "info:Show package information"
                "update:Update package repository"
                "self-update:Update the TSI binary itself"
                "doctor:Check system health"
                "remove:Uninstall TSI from the system"
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
                        "--verbose[Show full build output]" \
                        "*:package:->packages"
                    ;;
                uninstall)
                    _arguments \
                        "--prefix[Installation prefix]:directory:_files -/" \
                        "*:package:->installed_packages"
                    ;;
                upgrade)
                    _arguments \
                        "--prefix[Installation prefix]:directory:_files -/" \
                        "--verbose[Show full build output]" \
                        "*:package:->installed_packages"
                    ;;
                search)
                    _arguments \
                        "--prefix[Installation prefix]:directory:_files -/" \
                        "1:query: "
                    ;;
                info)
                    _arguments \
                        "--prefix[Installation prefix]:directory:_files -/" \
                        "*:package:->packages"
                    ;;
                update)
                    _arguments \
                        "--repo[Repository URL]:url:_urls" \
                        "--local[Local path]:directory:_files -/" \
                        "--prefix[Installation prefix]:directory:_files -/"
                    ;;
                self-update)
                    _arguments \
                        "--repo[Repository URL]:url:_urls" \
                        "--branch[Branch]" \
                        "--prefix[Installation prefix]:directory:_files -/"
                    ;;
                doctor|list)
                    _arguments \
                        "--prefix[Installation prefix]:directory:_files -/"
                    ;;
                remove)
                    _arguments \
                        "--prefix[Installation prefix to remove]:directory:_files -/" \
                        "--yes[Skip confirmation]"
                    ;;
                --help|--version|-h|-v)
                    _arguments
                    ;;
            esac
            ;;
    esac

    case $words[1] in
        install|info)
            if [[ $state == packages ]]; then
                local cur="${words[CURRENT]}"
                local repo_dir="${HOME}/.tsi/packages"
                if [[ $cur == *@ ]]; then
                    local pkg_name="${cur%@}"
                    if [[ -d "$repo_dir" && -n "$pkg_name" ]]; then
                        local versions=($(_tsi_versions "$repo_dir" "$pkg_name"))
                        if (( ${#versions[@]} > 0 )); then
                            local version_completions=()
                            for v in "${versions[@]}"; do
                                version_completions+=("${pkg_name}@${v}")
                            done
                            _describe 'version' version_completions
                        fi
                    fi
                elif [[ $cur == *@* ]]; then
                    local pkg_name="${cur%%@*}"
                    local version_part="${cur#*@}"
                    if [[ -d "$repo_dir" && -n "$pkg_name" ]]; then
                        local versions=($(_tsi_versions "$repo_dir" "$pkg_name"))
                        if (( ${#versions[@]} > 0 )); then
                            local version_completions=()
                            for v in "${versions[@]}"; do
                                [[ "$v" == "$version_part"* ]] && version_completions+=("${pkg_name}@${v}")
                            done
                            (( ${#version_completions[@]} > 0 )) && _describe 'version' version_completions
                        fi
                    fi
                else
                    if [[ -d "$repo_dir" ]]; then
                        local packages=($(ls -1 "$repo_dir"/*.json 2>/dev/null | xargs -n1 basename 2>/dev/null | sed 's/\.json$//' 2>/dev/null))
                        (( ${#packages[@]} > 0 )) && _describe 'package' packages
                    fi
                fi
            fi
            ;;
        uninstall|upgrade)
            if [[ $state == installed_packages ]]; then
                local installed=($(tsi list 2>/dev/null | grep -E "^  " | awk '{print $1}' | sed 's/://' 2>/dev/null))
                (( ${#installed[@]} > 0 )) && _describe 'installed package' installed
            fi
            ;;
    esac
}

compdef _tsi tsi
