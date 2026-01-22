#!/usr/bin/bash
NAMESPACE="${1:-codebase_b372_app}"
docker build -t "$NAMESPACE" .