Contributing
============

Contributions to this repository are welcome and encouraged.

All members of our community are expected to follow Read the Docs' `Code of Conduct`_. Please make sure you are welcoming and friendly in all of our spaces.

.. _Code of Conduct: https://docs.readthedocs.io/en/latest/code-of-conduct.html


Adding new themes
-----------------

Adding a new theme to this repository is relatively straight-forward. The steps include:

- Adding the branch name to the theme mapping to `conf.py` (see `branch_to_theme_mapping`). The name of the branch should closely follow the theme name.
- Updating the `README.rst` to link to the docs on Read the Docs
- Create an issue that a maintainer needs to create a new branch on the repository and configure Read the Docs to build docs on the new branch.
