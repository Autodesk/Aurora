# Contributing to Aurora

## Contributor License Agreement #
Before contributing code to this project, we ask that you sign the appropriate Contributor License Agreement (CLA):

+ [ADSKFormCorpContribAgmtForOpenSource.docx](Doc/CLA/ADSKFormCorpContribAgmtforOpenSource.docx): please sign this one for corporate use.
+ [ADSKFormIndContribAgmtForOpenSource.docx](Doc/CLA/ADSKFormIndContribAgmtforOpenSource.docx): please sign this one if you're an individual contributor

Return the completed form to aurora@autodesk.com. Once a signed form has been received you will be able to submit pull requests.

Contributors are expected to follow the [Contributor Covenant](CODE_OF_CONDUCT.md), which describes the code of conduct for Autodesk open source projects like this one.

## Logging Issues

### Suggestions

The Aurora project is meant to evolve with feedback - the project and its users greatly appreciate any thoughts on ways to improve the design or features. Please use the `enhancement` tag to specifically denote issues that are suggestions, as it helps us triage and respond appropriately.

### Bugs

As with all pieces of software, you may end up running into bugs. Please submit bugs as regular issues on GitHub. Aurora developers are regularly monitoring issues and will prioritize and schedule fixes.

The best bug reports include the following:

- Detailed steps to reliably reproduce the issue.
- A description of what the expected and the actual results are.
- Any screenshots or associated sample files to aid in reproducing the issue.
- What software versions and hardware is being used.
- Any other supplemental information that maybe useful in reproducing and diagnosing the issue.

## Contributing Code

The Aurora project accepts and greatly appreciates contributions. The project follows the [fork & pull](https://help.github.com/articles/using-pull-requests/#fork--pull) model for accepting contributions.

When contributing code, please also include appropriate tests as part of the pull request, and follow the published [coding standards](Doc/CodingStandards.md). In particular, use the same style as the code you are modifying.

Contributor pull requests should target the latest `contrib` branch of the repository, this will have a suffix of the latest released version, so contributions based on version v23.07 should target the `contrib/v23.07`. Please make sure the base branch of your pull request is set to the correct contrib branch when filing your pull request.  Contributor pull requests that are accepted will be merged into the contrib branch and then included in the next Aurora release from there. They will be merged into the main branch along with any other changes for the next release.

It is highly recommended that an issue be logged on GitHub before any work is started. This will allow for early feedback from other developers and will help avoid duplicate effort.
