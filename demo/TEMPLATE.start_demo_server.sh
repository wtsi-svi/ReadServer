#!/bin/bash

set -e

ssh READ_SERVER_IP 'nohup INST_BIN_DIR/service INST_DST/SERVER/service.cfg > INST_DST/SERVER/logs/log.service 2>&1 &'

ssh READ_SERVER_IP 'nohup INST_BIN_DIR/service_samples INST_DST/SERVER/service_samples.cfg > INST_DST/SERVER/logs/log.service_samples 2>&1 &'

ssh READ_SERVER_IP 'nohup INST_BIN_DIR/server INST_DST/SERVER/server.cfg > INST_DST/SERVER/logs/log.server 2>&1 &'
