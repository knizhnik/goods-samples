drop database knizhnik;
create database knizhnik;
ALTER DATABASE knizhnik SET search_path="$user",public,external_file;
