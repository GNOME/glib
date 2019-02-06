# CI support stuff

## Docker image

GitLab CI jobs run in a Docker image, defined here. To update that image
(perhaps to install some more packages):

1. Edit `.gitlab-ci/Dockerfile` with the changes you want
2. Edit `.gitlab-ci/run-docker.sh` and bump the version in `TAG`
3. Run `.gitlab-ci/run-docker.sh` to build the new image, and launch a shell
   inside it
    * When you're done, exit the shell in the usual way
4. Run `.gitlab-ci/run-docker.sh --push` to upload the new image to the GNOME
   GitLab Docker registry
    * If this is the first time you're doing this, you'll need to log into the
      registry
    * If you use 2-factor authentication on your GNOME GitLab account, you'll
      need to [create a personal access token][pat] and use that rather than
      your normal password
5. Edit `.gitlab-ci.yml` (in the root of this repository) to use your new
   image

[pat]: https://gitlab.gnome.org/profile/personal_access_tokens
