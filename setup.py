"""Setup script for TSI."""

from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name="tsi",
    version="0.1.0",
    author="Nico",
    description="TheSourceInstaller - Distribution-independent source-based package manager",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/PanterSoft/tsi",
    packages=find_packages(),
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "Intended Audience :: System Administrators",
        "License :: OSI Approved :: MIT License",
        "Operating System :: POSIX :: Linux",
        "Operating System :: Unix",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: System :: Archiving :: Packaging",
        "Topic :: System :: Software Distribution",
    ],
    python_requires=">=3.8",
    entry_points={
        "console_scripts": [
            "tsi=tsi.cli:main",
        ],
    },
    install_requires=[],
)

