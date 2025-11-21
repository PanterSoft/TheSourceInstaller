"""Dependency resolution for packages."""

from typing import Dict, List, Set
from collections import deque

from tsi.package import Package
from tsi.repository import Repository


class DependencyResolver:
    """Resolves package dependencies and build order."""

    def __init__(self, repository: Repository):
        """Initialize resolver with a repository."""
        self.repository = repository

    def resolve(self, package_name: str, installed: Set[str] = None) -> List[str]:
        """
        Resolve all dependencies for a package.
        Returns a list of package names in build order.
        """
        if installed is None:
            installed = set()

        resolved = []
        visited = set()
        processing = set()

        def visit(pkg_name: str):
            """Visit a package and its dependencies."""
            if pkg_name in installed:
                return

            if pkg_name in processing:
                raise ValueError(f"Circular dependency detected involving {pkg_name}")

            if pkg_name in visited:
                return

            processing.add(pkg_name)

            try:
                package = self.repository.get_package(pkg_name)
            except Exception as e:
                raise ValueError(f"Could not find package {pkg_name}: {e}")

            # Visit all dependencies first
            for dep in package.get_all_dependencies():
                visit(dep)

            processing.remove(pkg_name)
            visited.add(pkg_name)

            if pkg_name not in resolved:
                resolved.append(pkg_name)

        visit(package_name)
        return resolved

    def get_build_order(self, packages: List[str]) -> List[str]:
        """
        Get the correct build order for a list of packages.
        Uses topological sort.
        """
        # Build dependency graph
        graph: Dict[str, Set[str]] = {pkg: set() for pkg in packages}
        in_degree: Dict[str, int] = {pkg: 0 for pkg in packages}

        for pkg_name in packages:
            try:
                package = self.repository.get_package(pkg_name)
                for dep in package.get_all_dependencies():
                    if dep in packages:
                        graph[dep].add(pkg_name)
                        in_degree[pkg_name] += 1
            except Exception:
                # Package not found, skip
                pass

        # Topological sort
        queue = deque([pkg for pkg in packages if in_degree[pkg] == 0])
        result = []

        while queue:
            pkg = queue.popleft()
            result.append(pkg)

            for dependent in graph[pkg]:
                in_degree[dependent] -= 1
                if in_degree[dependent] == 0:
                    queue.append(dependent)

        if len(result) != len(packages):
            raise ValueError("Circular dependency detected in build order")

        return result

