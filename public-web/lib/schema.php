<?php
if (!defined('KFX_API_INTERNAL')) { http_response_code(404); exit; }
return <<<'SQL'

        PRAGMA journal_mode=WAL;
        CREATE TABLE IF NOT EXISTS users(id TEXT PRIMARY KEY,name TEXT UNIQUE COLLATE NOCASE,salt TEXT,password TEXT,public INTEGER DEFAULT 0,created REAL);
        CREATE TABLE IF NOT EXISTS recovery(user TEXT,code TEXT UNIQUE,PRIMARY KEY(user,code));
        CREATE TABLE IF NOT EXISTS sessions(token TEXT PRIMARY KEY,user TEXT,csrf TEXT,expires REAL);
        CREATE INDEX IF NOT EXISTS session_expiry ON sessions(expires);
        CREATE TABLE IF NOT EXISTS devices(code TEXT PRIMARY KEY,user_code TEXT UNIQUE,user TEXT,expires REAL,last_poll REAL DEFAULT 0);
        CREATE TABLE IF NOT EXISTS servers(id TEXT PRIMARY KEY,name TEXT,endpoint TEXT,engine TEXT,map TEXT,mods TEXT,players INTEGER,capacity INTEGER,password INTEGER,region TEXT,kind TEXT,owner TEXT,secret TEXT,last_seen REAL,status TEXT,rotation TEXT);
        CREATE TABLE IF NOT EXISTS friends(a TEXT,b TEXT,status TEXT,PRIMARY KEY(a,b));
        CREATE TABLE IF NOT EXISTS matches(id TEXT PRIMARY KEY,server TEXT,started REAL,ended REAL DEFAULT 0,map TEXT,engine TEXT,status TEXT DEFAULT 'playing',winner TEXT DEFAULT '',verification TEXT DEFAULT 'casual');
        CREATE TABLE IF NOT EXISTS participants(match TEXT,user TEXT,result TEXT DEFAULT '',PRIMARY KEY(match,user));
        CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY,owner TEXT,title TEXT,kind TEXT,starts REAL,description TEXT);
        CREATE TABLE IF NOT EXISTS bracket(event TEXT,round INTEGER,position INTEGER,a TEXT,b TEXT,winner TEXT,match TEXT UNIQUE,status TEXT,PRIMARY KEY(event,round,position));
        CREATE TABLE IF NOT EXISTS entrants(event TEXT,user TEXT,PRIMARY KEY(event,user));
        CREATE TABLE IF NOT EXISTS completions(user TEXT,challenge TEXT,match TEXT,PRIMARY KEY(user,challenge,match));
        CREATE TABLE IF NOT EXISTS challenges(id TEXT PRIMARY KEY,title TEXT,map TEXT,description TEXT);
        CREATE TABLE IF NOT EXISTS tickets(token TEXT PRIMARY KEY,server TEXT,user TEXT,expires REAL,peer INTEGER DEFAULT -1);
        CREATE TABLE IF NOT EXISTS identities(match TEXT,peer INTEGER,user TEXT,PRIMARY KEY(match,peer),UNIQUE(match,user));
        CREATE TABLE IF NOT EXISTS streams(id TEXT PRIMARY KEY,server TEXT,match TEXT,data BLOB,finished INTEGER,engine TEXT);
        CREATE TABLE IF NOT EXISTS replays(id TEXT PRIMARY KEY,match TEXT,owner TEXT,data BLOB,sha TEXT,size INTEGER);
        
CREATE TABLE IF NOT EXISTS rate_limits(peer TEXT,bucket TEXT,start REAL,count INTEGER,PRIMARY KEY(peer,bucket));
SQL;
