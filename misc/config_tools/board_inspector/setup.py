from setuptools import setup, find_packages

setup(
    name="acrn-board-inspector",
    version=3.0,
    description="Collecting board information for building ACRN hypervisor",
    long_description=open("README.md").read(),
    long_description_content_type='text/markdown',
    author="Intel",
    author_email="acrn-dev@lists.projectacrn.org",
    license="BSD-3-Clause",
    url="https://github.com/projectacrn/acrn-hypervisor",
    python_requires='>=3.6',
    classifiers=[
        "License :: OSI Approved :: BSD-3-Clause License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.6",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Operating System :: POSIX",
        "Private :: Do Not Upload",
    ],
    project_urls={
        'Source': 'https://github.com/projectacrn/acrn-hypervisor',
        'Bug Tracker': 'https://github.com/projectacrn/acrn-hypervisor/issues',
    },
    install_requires=[
        "lxml",
    ],
    packages=["acrn_board_inspector"] + list(map(lambda x: "acrn_board_inspector." + x, find_packages())),
    package_dir={
        "acrn_board_inspector": ".",
    }
)
