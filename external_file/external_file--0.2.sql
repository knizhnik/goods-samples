-- External files extension for PostgreSQL
-- Author Dominique Legendre
-- Copyright (c) 2012-2015 Brgm - All rights reserved.

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION external_file" to load this file. \quit


CREATE OR REPLACE FUNCTION writeEfile(buffer bytea, oid bigint)
  RETURNS void
AS $$
DECLARE
  l_oid oid;
  lfd integer;
  lsize integer;
BEGIN
  l_oid := lo_create(0);
  lfd := lo_open(l_oid,131072); --0x00020000 write mode
  lsize := lowrite(lfd,buffer);
  PERFORM lo_close(lfd);
  PERFORM lo_export(l_oid,'pg_blob/' || oid);
  PERFORM lo_unlink(l_oid);
END;
$$ LANGUAGE PLPGSQL SECURITY DEFINER SET search_path = @extschema@, pg_temp;


CREATE OR REPLACE FUNCTION readEfile(oid bigint, p_result OUT bytea)
AS $$
DECLARE
  l_oid oid;
  r record;
BEGIN
  p_result := '';
  SELECT lo_import('pg_blob/' || oid) INTO l_oid;
  FOR r IN ( SELECT data 
             FROM pg_largeobject 
             WHERE loid = l_oid 
             ORDER BY pageno ) LOOP
    p_result = p_result || r.data;
  END LOOP;
  PERFORM lo_unlink(l_oid);
END;
$$
LANGUAGE PLPGSQL SECURITY DEFINER SET search_path = @extschema@, pg_temp;
