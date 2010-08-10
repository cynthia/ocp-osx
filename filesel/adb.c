/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Archive DataBase
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * revision history: (please note changes here)
 *  -ss040823   Stian Skjelstad <stian@nixia.no
 *    -first release (splited out from pfilesel.c)
 *  -ss040915   Stian Skjelstad <stian@nixia.no>
 *    -Make sure that the adb_ReadHandle does not survive a fork
 *  -ss040918   Stian Skjelstad <stian@nixia.no>
 *    -Add "Scanning archive" message to the console
 *  -ss050124   Stian Skjelstad <stian@nixia.no>
 *    -fnmatch into place
 */

#include "config.h"
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "types.h"
#include "adb.h"
#include "dirdb.h"
#include "boot/plinkman.h"
#include "stuff/poutput.h"
#include "boot/psetting.h"
#include "modlist.h"
#include "mdb.h"
#include "pfilesel.h"
#include "stuff/compat.h"

static struct adbregstruct *adbPackers = 0;
static char adbDirty;
static struct arcentry *adbData;
static uint32_t adbNum;
static uint32_t adbFindArc;
static uint32_t adbFindPos;

const char adbsigv1[16] = "CPArchiveCache\x1B\x00";
const char adbsigv2[16] = "CPArchiveCache\x1B\x01";

struct __attribute__((packed)) adbheader
{
	char sig[16];
	uint32_t entries;
};

void adbRegister(struct adbregstruct *r)
{
	r->next=adbPackers;
	adbPackers=r;
}

void adbUnregister(struct adbregstruct *r)
{
	struct adbregstruct *root=adbPackers;
	if (root==r)
	{
		adbPackers=r->next;
		return;
	}
	while (root)
	{
		if (root->next==r)
		{
			root->next=root->next->next;
			return;
		}
		if (!root->next)
			return;
		root=root->next;
	}
}

char adbInit(void)
{
	char path[PATH_MAX+1];
	int f;
	struct adbheader header;

	int old=0;
	unsigned int i;

      	adbDirty=0;
	adbData=0;
	adbNum=0;
	if ((strlen(cfConfigDir)+10)>=PATH_MAX)
		return 1; /* path is too long */
	strcpy(path, cfConfigDir);
      	strcat(path, "CPARCS.DAT");

	if ((f=open(path, O_RDONLY))<0)
		return 1;

	fprintf(stderr, "Loading %s .. ", path);

	if (read(f, &header, sizeof(header))!=sizeof(header))
	{
		fprintf(stderr, "No header\n");
		close(f);
		return 1;
	}

	if (!memcmp(header.sig, adbsigv1, 16))
	{
		old=1;
		fprintf(stderr, "(Old format)  ");
	} else if (memcmp(header.sig, adbsigv2, 16))
	{
		fprintf(stderr, "Invalid header\n");
		close(f);
		return 1;
	}

	adbNum=uint32_little(header.entries);
	if (!adbNum)
	{
		fprintf(stderr, "Cache empty\n");
		close(f);
		return 1;
	}
	adbData=malloc(sizeof(struct arcentry)*adbNum);
	if (!adbData)
		return 0;
	if (old)
	{
		#define OLD_ARC_PATH_MAX 63
		struct __attribute__((packed)) oldarcentry
		{
			uint8_t flags;
			uint32_t parent;
			char name[OLD_ARC_PATH_MAX+1]; /* some stupid archives needs full path, which can be long */
			uint32_t size;
		} oldentry;
		for (i=0;i<adbNum;i++)
		{
			if (read(f, &oldentry, sizeof(oldentry))!=(sizeof(oldentry)))
			{
				fprintf(stderr, "EOF\n");
				/* File is broken / premature EOF */
				free(adbData);
				adbData=0;
				adbNum=0;
				close(f);
				return 1;
			}
			adbData[i].flags=oldentry.flags;
			adbData[i].parent=uint32_little(oldentry.parent);
			strncpy(adbData[i].name, oldentry.name, sizeof(adbData[i].name));
			adbData[i].name[sizeof(adbData[i].name)-1]=0;
			adbData[i].size=uint32_little(oldentry.size);
		}
	} else {
		if (read(f, adbData, adbNum*sizeof(*adbData))!=(ssize_t)(adbNum*sizeof(*adbData)))
		{
			fprintf(stderr, "EOF\n");
			/* File is broken / premature EOF */
			free(adbData);
			adbData=0;
			adbNum=0;
			close(f);
			return 1;
		}
		for (i=0;i<adbNum;i++)
		{
			adbData[i].parent=uint32_little(adbData[i].parent);
			adbData[i].size=uint32_little(adbData[i].size);
		}
	}
	close(f);
	fprintf(stderr, "Done\n");
	return 1;
}

void adbUpdate(void)
{
	char path[PATH_MAX+1];
	int f;
	uint32_t i, j;
	struct adbheader header;

	if (!adbDirty)
		return;
	adbDirty=0;

	if ((strlen(cfConfigDir)+10)>=PATH_MAX)
		return; /* path is too long */
	strcpy(path, cfConfigDir);
      	strcat(path, "CPARCS.DAT");

	if ((f=open(path, O_WRONLY|O_CREAT, S_IREAD|S_IWRITE))<0)
	{
		perror("open(CPARCS.DAT");
		return;
	}

	lseek(f, 0, SEEK_SET);
	memcpy(header.sig, adbsigv2, 16);
	header.entries=uint32_little(adbNum);
	while (1)
	{
		ssize_t res;
		res = write(f, &header, sizeof(header));
		if (res < 0)
		{
			if (errno==EAGAIN)
				continue;
			if (errno==EINTR)
				continue;
			fprintf(stderr, __FILE__ " write() to %s failed: %s\n", path, strerror(errno));
			exit(1);
		} else if (res != sizeof(header))
		{
			fprintf(stderr, __FILE__ " write() to %s returned only partial data\n", path);
			exit(1);
		} else
			break;
	}

	i=0;

	while (i<adbNum)
	{
		if (!(adbData[i].flags&ADB_DIRTY))
		{
			i++;
			continue;
		}

		for (j=i; j<adbNum; j++)
			if (adbData[j].flags&ADB_DIRTY)
				adbData[j].flags&=~ADB_DIRTY;
			else
				break;
		lseek(f, 20+i*sizeof(*adbData), SEEK_SET);
		adbData[i].parent=uint32_little(adbData[i].parent);
		adbData[i].size=uint32_little(adbData[i].size);
		while (1)
		{
			ssize_t res;
			res = write(f, adbData+i, (j-i)*sizeof(*adbData));
			if (res < 0)
			{
				if (errno==EAGAIN)
					continue;
				if (errno==EINTR)
					continue;
				fprintf(stderr, __FILE__ " write() to %s failed: %s\n", path, strerror(errno));
				exit(1);
			} else if (res != (ssize_t)((j-i)*sizeof(*adbData)))
			{
				fprintf(stderr, __FILE__ " write() to %s returned only partial data\n", path);
				exit(1);
			} else
				break;
		}
		adbData[i].parent=uint32_little(adbData[i].parent);
		adbData[i].size=uint32_little(adbData[i].size);
		i=j;
	}
	lseek(f, 0, SEEK_END);
	close(f);
}

void adbClose(void)
{
	adbUpdate();
	free(adbData);
	adbPackers=0;
}

int adbAdd(const struct arcentry *a)
{
	uint32_t i;

	for (i=0; i<adbNum; i++)
		if (!(adbData[i].flags&ADB_USED))
			break;

	if (i==adbNum)
	{
		void *t;
		uint32_t j;
		
		adbNum+=256;
		if (!(t=realloc(adbData, adbNum*sizeof(*adbData))))
			return 0;
		adbData=(struct arcentry*)t;
		memset(adbData+i, 0, (adbNum-i)*sizeof(*adbData));
		for (j=i; j<adbNum; j++)
			adbData[j].flags|=ADB_DIRTY;
	}
	memcpy(&adbData[i], a, sizeof(struct arcentry));
	adbData[i].flags|=ADB_USED|ADB_DIRTY;
	if (a->flags&ADB_ARC)
		adbData[i].parent=i;
	adbDirty=1;
	return 1;
}

uint32_t adbFind(const char *arcname)
{
	uint32_t i;
	size_t len=strlen(arcname)+1;
	for (i=0; i<adbNum; i++)
		if ((adbData[i].flags&(ADB_USED|ADB_ARC))==(ADB_USED|ADB_ARC))
			if (!memcmp(adbData[i].name, arcname, len))
				return i;
	return 0xFFFFFFFF;
}

int adbCallArc(const char *cmd, const char *arc, const char *name, const char *dir)
{
	char *argv[256];
	int argc=0;

	char *space;

	pid_t child;
	int count;
	int retval;

	if (!(space=strchr(cmd, ' ')))
		return EINVAL;

	if (!(argv[0]=malloc(space-cmd+1)))
		goto fail;
	strncpy(argv[0], cmd, space-cmd);
	argv[0][space-cmd]=0;
	argc=1;
	argv[argc]=0;

	cmd=space;
	while (*cmd)
	{
		if ((*cmd)==' ')
		{
			if (argv[argc])
			{
				if (argc>=254)
				{
					retval=E2BIG;
					goto fail;
				}
				argv[++argc]=0;
			}
		} else if ((*cmd)=='%')
		{
			cmd++;
			switch (*cmd)
			{
				default:
				case 0:
					retval=EINVAL;
					goto fail;
				case 'a':
				case 'A':
					if (!argv[argc])
					{
						if (!(argv[argc]=malloc(strlen(arc)+1)))
						{
							retval=ENOMEM;
							goto fail;
						}
						strcpy(argv[argc], arc);
					} else {
						char *tmp;
						if (!(tmp=realloc(argv[argc], strlen(argv[argc])+strlen(arc)+1)))
						{
							retval=ENOMEM;
							goto fail;
						}
						strcat(argv[argc], arc);
					}
					break;
				case 'n':
				case 'N':
					if (!argv[argc])
					{
						if (!(argv[argc]=malloc(strlen(name)+1)))
						{
							retval=ENOMEM;
							goto fail;
						}
						strcpy(argv[argc], name);
					} else {
						char *tmp;
						if (!(tmp=realloc(argv[argc], strlen(argv[argc])+strlen(name)+1)))
						{
							retval=ENOMEM;
							goto fail;
						}
						strcat(argv[argc], name);
					}
					break;
				case 'd':
				case 'D':
					if (!argv[argc])
					{
						if (!(argv[argc]=malloc(strlen(dir)+1)))
						{
							retval=ENOMEM;
							goto fail;
						}
						strcpy(argv[argc], dir);
					} else {
						char *tmp;
						if (!(tmp=realloc(argv[argc], strlen(argv[argc])+strlen(dir)+1)))
						{
							retval=ENOMEM;
							goto fail;
						}
						strcat(argv[argc], dir);
					}
					break;
				case '%':
					if (!argv[argc])
					{
						if (!(argv[argc]=malloc(2)))
						{
							retval=ENOMEM;
							goto fail;
						}
						argv[argc][0]='%';
						argv[argc][1]=0;
					} else {
						char *tmp;
						int len;
						if (!(tmp=realloc(argv[argc], (len=strlen(argv[argc]))+2)))
						{
							retval=ENOMEM;
							goto fail;
						}
						argv[argc]=tmp;
						argv[argc][len++]='%';
						argv[argc][len]=0;
					}
					break;
			}
		} else {
			if (!argv[argc])
			{
				if (!(argv[argc]=malloc(2)))
				{
					retval=ENOMEM;
					goto fail;
				}
				argv[argc][0]=*cmd;
				argv[argc][1]=0;
			} else {
				char *tmp;
				int len;
				if (!(tmp=realloc(argv[argc], (len=strlen(argv[argc]))+2)))
				{
					retval=ENOMEM;
					goto fail;
				}
				argv[argc]=tmp;
				argv[argc][len++]=*cmd;
				argv[argc][len]=0;
			}	
		}
		cmd++;
	}

	if (argv[argc])
		argv[++argc]=0;

	if ((child=fork()))
	{
		if (child<0)
		{
			retval=errno;
			goto fail;
		}
		while (waitpid(child, &retval, 0)!=child)
		{
			if (errno!=EINTR)
			{
				retval=errno;
				goto fail;
			}
		}
	} else {
		execvp(argv[0], argv);
		exit(errno);
	}
fail:
	for (count=0;count<=argc;count++)
		if (argv[count])
			free(argv[count]);
	return retval;
}

static int isarchive(const char *ext)
{
	struct adbregstruct *packers;
	for (packers=adbPackers; packers; packers=packers->next)
		if (!strcasecmp(ext, packers->ext))
			return 1;
	return 0;
}

int isarchivepath(const char *p)
{
	char path[PATH_MAX+1];
	char ext[NAME_MAX+1];
/*	struct stat st;*/

	strcpy(path, p);
	if (*p)
		if (path[strlen(path)-1]=='/')
			path[strlen(path)-1]=0;
	
/*	if (stat(path, &st))
		return 0;
	if (!S_ISREG(st.st_mode))
		return 0;*/

	_splitpath(path, 0, 0, 0, ext);

	return isarchive(ext);
}

static signed char adbFindNext(char *findname, uint32_t *findlen, uint32_t *adb_ref)
{
	uint32_t i;
	for (i=adbFindPos; i<adbNum; i++)
		if ((adbData[i].flags&(ADB_USED|ADB_ARC))==ADB_USED)
			if (adbData[i].parent==adbFindArc)
			{
				strcpy(findname, adbData[i].name);
				*findlen=adbData[i].size;
				adbFindPos=i+1;
				*adb_ref=i;
				return 0;
			}
	return 1;
}

static signed char adbFindFirst(const char *path, unsigned long arclen, char *findname, uint32_t *findlen, uint32_t *adb_ref)
{
	char ext[NAME_MAX+1]; /* makes life safe */
	char name[NAME_MAX+1]; /* makes life safe */
	char arcname[ARC_PATH_MAX+1]; /* makes life safe */
	uint32_t ar, i;

	_splitpath(path, 0, 0, name, ext);
	if ((strlen(name)+strlen(ext))>ARC_PATH_MAX)
		return -1;

	strcpy(arcname, name);
	strcat(arcname, ext);

	ar=adbFind(arcname);
	
	if ((ar==0xFFFFFFFF)||(arclen!=(ar!=0xffffffff?adbData[ar].size:0)))
	{
		if (ar!=0xFFFFFFFF)
			for (i=0; i<adbNum; i++)
				if (adbData[i].parent==ar)
					adbData[i].flags=(adbData[i].flags|ADB_DIRTY)&~ADB_USED;
		adbDirty=1;
		{
			struct adbregstruct *packers;
			for (packers=adbPackers; packers; packers=packers->next)
				if (!strcasecmp(ext, packers->ext))
				{
					conRestore();
					if (!packers->Scan(path))
						return -1;
					else
						break;
				}
			if (!packers)
				return 1;
		}
		ar=adbFind(arcname);
	}
	adbFindArc=ar;
	adbFindPos=0;
	return adbFindNext(findname, findlen, adb_ref);
}




FILE *adb_ReadHandle(struct modlistentry *entry)
{
	char dir[PATH_MAX+1];
	int fd;
	char path[PATH_MAX+1];
	char temppath[PATH_MAX+1];
	char ext[NAME_MAX+1];

	char npath[PATH_MAX+1];

	struct arcentry *this/*, *parent*/;
	this=&adbData[entry->adb_ref];

	dirdbGetFullName(entry->dirdbfullpath, npath, DIRDB_FULLNAME_NOBASE);

	_splitpath(npath, NULL, dir, NULL, NULL);

	/* TODO, logic here should work / for / and stat for dir/file */
	
	_makepath(path, 0, dir, 0, 0);
	path[strlen(path)-1]=0; /* Remove / suffix */

	if (!isarchivepath(path))
		return 0;

	if ((strlen(cfTempDir)+strlen("ocptmpXXXXXX"))>PATH_MAX)
		return 0;
	
	_splitpath(path, 0, 0, 0, ext);

	strcpy(temppath, cfTempDir);
	strcat(temppath, "ocptmpXXXXXX");

	if ((fd=mkstemp(temppath))<0)
	{
		perror("adc.c: mkstemp()");
		return 0;
	}
	/*fcntl(fd, F_SETFD, 1<<FD_CLOEXEC);*/

	{
		struct adbregstruct *packers;
		for (packers=adbPackers; packers; packers=packers->next)
			if (!strcasecmp(ext, packers->ext))
			{
				if (!packers->Call(adbCallGet, path, this->name, fd))
				{
					close(fd);
					unlink(temppath);
					fprintf(stderr, "adb.c: Failed to fetch file\n");
					return 0;
				} else {
					lseek(fd, 0, SEEK_SET);
					unlink(temppath);
					return fdopen(fd, "r");
				}
			}
	}
	fprintf(stderr, "adc.c: No packer found?\n");
	close(fd);
	return 0;
}

int adb_ReadHeader(struct modlistentry *entry, char *mem, size_t *size)
{
	FILE *f=adb_ReadHandle(entry);
	int res;
	if (!f)
		return -1;
	res=fread(mem, 1, *size, f);
	*size=res;
	fclose(f);
	return 0;
}

int adb_Read(struct modlistentry *entry, char **mem, size_t *size)
{
	FILE *f=adb_ReadHandle(entry);
	int res;
	if (!f)
		return -1;

	*mem=malloc(*size=(1024*1024*128));
	res=fread(mem, 1, *size, f);
	if (!(*size=res))
	{
		free(*mem);
		*mem=NULL;
	} else {
		*mem=realloc(*mem, *size);
	}
	
	fclose(f);
	return 0;
}

static int arcReadDir(struct modlist *ml, const struct dmDrive *drive, const uint32_t path, const char *mask, unsigned long opt)
{
	char _path[PATH_MAX+1];

	struct modlistentry m;

	if (drive!=dmFILE)
		return 1;

	dirdbGetFullName(path, _path, DIRDB_FULLNAME_NOBASE);
/*	dmGetPath(path, dirref);*/
      	if (isarchivepath(_path))
      	{
/*		dmGetPath(path, dirref);
	    	path[strlen(path)-1]=0;*/

		size_t flength;
		int tf;

		uint32_t size, adb_ref;
		signed char done;

		char newfilepath[ARC_PATH_MAX+1];
	
#ifndef FNM_CASEFOLD
		char *mask_upper;
		char *iterate;

		if ((mask_upper = strdup(mask)))
		{
			for (iterate = mask_upper; *iterate; iterate++)
				*iterate = toupper(*iterate);
		} else {
			perror("adb.c: strdup() failed");
			return 1;
		}
#endif

		if ((tf=open(_path, O_RDONLY))<0)
			if (tf<0)
			{
#ifndef FNM_CASEFOLD
				free(mask_upper);
#endif
				return 1;
			}
		flength=filelength(tf);
		close(tf);

	    	for (done=adbFindFirst(_path, flength, newfilepath, &size, &adb_ref); !done; done=adbFindNext(newfilepath, &size, &adb_ref))
		{
			char name[NAME_MAX+1];
			char ext[NAME_MAX+1];
			char npath[PATH_MAX+1];

			char *tmp=rindex(newfilepath, '/');
			if (!tmp)
				tmp=newfilepath;
			else
				tmp++;
#ifndef FNM_CASEFOLD
			{
				char *tmp_upper;

				if ((tmp_upper = strdup(tmp)))
				{
					for (iterate = tmp_upper; *iterate; iterate++)
						*iterate = toupper(*iterate);
				} else {
					perror("adb.c: strdup() failed");
					continue;
				}

				if (fnmatch(mask_upper, tmp_upper, 0))
				{
					free(tmp_upper);
					continue;
				}
				free(tmp_upper);
			}
#else
			if (fnmatch(mask, tmp, FNM_CASEFOLD))
				continue;
#endif

			_splitpath(newfilepath, 0, 0, name, ext);
			strcpy(m.name, newfilepath);

			m.drive=drive;
			_makepath(npath, 0, _path, name, ext);
			m.dirdbfullpath=dirdbResolvePathWithBaseAndRef(drive->basepath, npath);
			if ((strlen(name)+strlen(ext))<NAME_MAX)
				strcat(name, ext);
			m.flags=MODLIST_FLAG_FILE|MODLIST_FLAG_VIRTUAL;
			m.Read=adb_Read;
			m.ReadHeader=adb_ReadHeader;
			m.ReadHandle=adb_ReadHandle;
			fs12name(m.shortname, name);
			m.fileref=mdbGetModuleReference(m.shortname, size);
			m.adb_ref=adb_ref;
#if 0
			m.modinfo.gen.size=size;
			m.modinfo.gen.modtype=mtUnRead; /* TODO mdb etc. */
#endif

			modlist_append(ml, &m);
			dirdbUnref(m.dirdbfullpath);
		}
#ifndef FNM_CASEFOLD
		free(mask_upper);
#endif
		if (done==-1)
			return 0;
	}
	return 1;
}

struct mdbreaddirregstruct adbReadDirReg = {arcReadDir MDBREADDIRREGSTRUCT_TAIL};
