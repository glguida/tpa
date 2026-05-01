from setuptools import find_packages, setup


setup(
    name="tpa-planner",
    version="0.1.0",
    description="Offline TPA process metadata extraction and program planning tools",
    packages=find_packages("src"),
    package_dir={"": "src"},
    install_requires=["networkx"],
    entry_points={
        "console_scripts": [
            "tpa-extract-process-json=tpa_planner.extract_process_metadata:main",
            "tpa-plan-program=tpa_planner.plan_program:main",
            "tpa-map-program=tpa_planner.map_program:main",
        ]
    },
)
