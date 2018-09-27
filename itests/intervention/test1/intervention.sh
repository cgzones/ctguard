#!/bin/sh

sleep 1

echo "std output"
(>&2 echo "std error")
echo "success '$1'" >> intervention_action.log
