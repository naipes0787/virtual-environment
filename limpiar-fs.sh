#!/bin/bash

PATHFIFA="/home/utnso/fifa-examples"
PATHBACKUP="/home/utnso/fifa-examples-master"
rm -rf $PATHFIFA/*
cp -r $PATHBACKUP/fifa-checkpoint $PATHFIFA/
cp -r $PATHBACKUP/fifa-entrega $PATHFIFA/
