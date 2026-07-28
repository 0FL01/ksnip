#!/usr/bin/env bash

set -euo pipefail

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
	echo "Usage: $0 <destination> [--offline]" >&2
	exit 2
fi

destination=$1
mode=${2:-}
script_dir=$(dirname "$(readlink -f "$0")")
lock_file="$script_dir/ocr-sources.lock"

if [ "$mode" != "" ] && [ "$mode" != "--offline" ]; then
	echo "Unknown mode: $mode" >&2
	exit 2
fi

mkdir -p "$destination"

while read -r sha256 filename url; do
	case "$sha256" in
		""|\#*) continue ;;
	esac

	output="$destination/$filename"
	if ! printf '%s  %s\n' "$sha256" "$output" | sha256sum --check --status; then
		if [ "$mode" = "--offline" ]; then
			echo "Missing or invalid offline OCR source: $output" >&2
			exit 1
		fi

		partial="$output.part"
		rm -f "$partial"
		curl --fail --location --retry 3 --output "$partial" "$url"
		mv "$partial" "$output"
	fi

	printf '%s  %s\n' "$sha256" "$output" | sha256sum --check
done < "$lock_file"
