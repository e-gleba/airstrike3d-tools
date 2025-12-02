# sudo apt-get install python3-dev
cpmaddpackage(
    NAME pybind11
    GITHUB_REPOSITORY pybind/pybind11
    GIT_TAG v3.0.1
    SYSTEM ON
    GIT_SHALLOW ON
        OPTIONS
        "PYBIND11_INSTALL OFF"
        "PYBIND11_TEST OFF"
        "PYBIND11_NOPYTHON OFF"
        "PYBIND11_FINDPYTHON ON"
)
