#!/bin/bash

set -e

ssh host-name-or.ip.address 'nohup /path/to/readserver/install/directory/libs/bin/service /path/to/readserver/install/directory/demo/service.cfg > /path/to/log/directory/log.service 2>&1 &'

ssh host-name-or.ip.address 'nohup /path/to/readserver/install/directory/libs/bin/service_samples /path/to/readserver/install/directory/demo/service_samples.cfg > /path/to/log/directory/log.service_samples 2>&1 &'

ssh host-name-or.ip.address 'nohup /path/to/readserver/install/directory/libs/bin/server /path/to/readserver/install/directory/demo/server.cfg > /path/to/log/directory/log.server 2>&1 &'

