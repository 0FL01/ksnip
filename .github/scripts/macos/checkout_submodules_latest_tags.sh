#!/bin/bash

set -euo pipefail

SUBMODULES=(
  "libraries/kImageAnnotator"
  "libraries/kColorPicker"
)

for submodule in "${SUBMODULES[@]}"; do
  if [[ ! -d "${submodule}/.git" ]]; then
    echo "Submodule ${submodule} is not initialized. Initializing..."
    git submodule update --init --recursive "${submodule}"
  fi

  if [[ "${submodule}" == "libraries/kImageAnnotator" ]]; then
    # This remote also contains application tags. The modified editor must use
    # the superproject's pinned library commit, never the latest application tag.
    echo "Keeping ${submodule} at the pinned editor revision"
    continue
  fi

  echo "Fetching tags for ${submodule}"
  git -C "${submodule}" fetch --tags --force

  LATEST_TAG=$(git -C "${submodule}" tag --list "v*" --sort=-v:refname | head -n 1)
  if [[ -z "${LATEST_TAG}" ]]; then
    echo "No version tags found for ${submodule}"
    exit 1
  fi

  echo "Checking out ${submodule} at ${LATEST_TAG}"
  git -C "${submodule}" checkout -q "${LATEST_TAG}"
done
