#!/bin/bash

ssh READ_SERVER_IP 'ps -C service | grep "service" | awk '"'"'{print $1}'"'"' | xargs --no-run-if-empty kill -9'

ssh READ_SERVER_IP 'ps -C service_samples | grep "service_samples" | awk '"'"'{print $1}'"'"' | xargs --no-run-if-empty kill -9'

ssh READ_SERVER_IP 'ps -C server | grep "server" | awk '"'"'{print $1}'"'"' | xargs --no-run-if-empty kill -9'
