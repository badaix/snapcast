[Describe your pull request here. Please read the text below the line, and make sure you follow the checklist.]

* * *

## Pull Request Checklist

* Contributions must be licensed under the [GPL-3.0 License](LICENSE)

* This project loosely follows the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)

* For better compatibility with embedded toolchains, the used C++ standard should be limited to C++17

* Code should be formatted by running `make reformat`

* Branch from the `develop` branch and ensure it is up to date with the current `develop` branch before submitting your pull request. If it doesn't merge cleanly with `develop`, you may be asked to resolve the conflicts. Pull requests to master will be closed.

* Commits should be as small as possible while ensuring that each commit is correct independently (i.e., each commit should compile and pass tests).

* Pull requests must not contain compiled sources (already set by the default .gitignore) or binary files

* Test your changes as thoroughly as possible before you commit them. Preferably, automate your test by unit/integration tests. If tested manually, provide information about the test scope in the PR description (e.g. “Test passed: Upgrade version from 0.42 to 0.42.23.”).

* Create _Work In Progress [WIP]_ pull requests only if you need clarification or an explicit review before you can continue your work item.

* If your patch is not getting reviewed or you need a specific person to review it, you can @-reply a reviewer asking for a review in the pull request or a comment, or you can ask for a review by contacting us via [email](mailto:snapcast@badaix.de).

* Post review:
  * If a review requires you to change your commit(s), please test the changes again.
  * Amend the affected commit(s) and force push onto your branch.
  * Set respective comments in your GitHub review to resolved.
  * Create a general PR comment to notify the reviewers that your amendments are ready for another round of review.
