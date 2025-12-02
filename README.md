# README

--- 

Use this markdown to keep track of what we are all currently working on in order to prevent confusion and rewriting code.

Once a module has been finished, go to tests/test-config.txt and change the status of the module from 'WIP' to 'DONE'.

## Branching

We should use branches to ensure code isn't written over:
To create a branch: 
```bash
git checkout -b <branch-name>
```
you can use `git branch` to determine which branch you are on

## Committing

WHEN COMMITTING CODE:  
`git add .` (adds all files, change '.' to specific file to commit only one)  
`git commit -m "<msg>"` **IMPORTANT:** use an appropriate commit message so other memebers can guage what you are working on  
`git push -u origin <branch-name>` please dont force push to main branch or even push to main branch  

## Merging

When your code is done and you want to merge to main branch:
1. Go to gitlabs and open a Merge Request
2. Select your branch as Source and Main branch as destination
3. Submit

GOLDEN RULE: NEVER ACCEPT YOUR OWN PR

This will allow all of us to review each others code before anything is committed to main branch, which will make for easier group contributions and ensure correctness

Once all members have reviewed merge request, and agree that the code is working as intended, we will merge the branch to main. 

## Gitlabs Issues

Gitlabs has a feature (the exact same as github) called Issues. If you are struggling with something and want a second opinion, post to the issues tab. This will signal other group members that you need help. 

---

## Necessary Implements (NOW): 

- designdoc Abstractions plan
- designdoc Parallelization plan

---

## Necessary Implements (Lower Priority):

- Makefile
- main.c 
- connection.c
- dispatcher.c
- file.c
- http.c
- log.c
- parallel.c
- tests/run_test.sh
- .gitlab-ci.yml

---

## Currently Working:

**David**: Creating file structure and testing scripts. 

**Nikhil**: Working on connection

**Koa**:

