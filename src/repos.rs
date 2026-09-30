//! The GitHub repositories TSI talks to, kept in one place so a rename is a
//! one-line change. TSI's own repository comes from `repository` in Cargo.toml.

/// TSI's own repository: releases (pre-built binaries) and source.
pub const TSI_REPO: &str = env!("CARGO_PKG_REPOSITORY");

/// The official package definitions, fetched by `tsi update`.
pub const PACKAGES_REPO: &str = "https://github.com/PanterSoft/tsi-packages.git";

/// `owner/name` of a GitHub repository URL (`https://github.com/owner/name[.git][/]`),
/// or `None` for anything that is not a GitHub URL.
pub fn github_slug(repo: &str) -> Option<String> {
    let rest = repo
        .strip_prefix("https://github.com/")
        .or_else(|| repo.strip_prefix("http://github.com/"))?;
    let slug = rest.trim_end_matches('/').trim_end_matches(".git");
    let mut parts = slug.split('/');
    match (parts.next(), parts.next(), parts.next()) {
        (Some(owner), Some(name), None) if !owner.is_empty() && !name.is_empty() => {
            Some(slug.to_string())
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tsi_repo_is_the_current_github_repository() {
        assert_eq!(
            github_slug(TSI_REPO).as_deref(),
            Some("PanterSoft/TheSourceInstaller")
        );
        assert_eq!(
            github_slug(PACKAGES_REPO).as_deref(),
            Some("PanterSoft/tsi-packages")
        );
    }

    #[test]
    fn github_slug_forms() {
        for url in [
            "https://github.com/o/r",
            "https://github.com/o/r.git",
            "https://github.com/o/r/",
            "http://github.com/o/r",
        ] {
            assert_eq!(github_slug(url).as_deref(), Some("o/r"), "{url}");
        }
        assert_eq!(github_slug("https://gitlab.com/o/r.git"), None);
        assert_eq!(github_slug("https://github.com/o"), None);
        assert_eq!(github_slug("https://github.com/o/r/tree/main"), None);
    }
}
