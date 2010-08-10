/* OpenCP Module Player
 * copyright (c) '94-'10 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *
 * Link manager - contains high-level routines to load and handle
 *                external DLLs
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
 *  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
 *    -first release
 *  -kb980717   Tammo Hinrichs <opencp@gmx.net>
 *    -added lnkReadInfoReg() to read out _dllinfo entries
 *  -fd981014   Felix Domke    <tmbinc@gmx.net>
 *    -increased the dllinfo-buffersize from 256 to 1024 chars in parseinfo
 *  -fd981206   Felix Domke    <tmbinc@gmx.net>
 *    -edited for new binfile
 *  -ryg981206  Fabian Giesen  <fabian@jdcs.su.nw.schule.de>
 *    -added DLL autoloader (DOS only)
 *  -ss040613   Stian Skjelstad  <stian@nixia.no>
 *    -rewritten for unix
 *  -ss040831   Stian Skjelstad  <stian@nixia.no>
 *    -made lnkDoLoad more strict, and work correct when LD_DEBUG is not defined
 *  -doj040907  Dirk Jagdmann  <doj@cubic.org>
 *    -better error message of dllextinfo is not found
 */

#include "config.h"
#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "types.h"

#include "psetting.h"
#include "plinkman.h"

static int handlecounter;
struct dll_handle loadlist[MAXDLLLIST];
int loadlist_n;

#ifdef SUPPORT_STATIC_PLUGINS
DLLEXTINFO_PREFIX struct linkinfostruct staticdlls = {"static", "Compiled in plugins (c) 2009 Stian Skjelstad", DLLVERSION, 0 LINKINFOSTRUCT_NOEVENTS};
#endif

static char reglist[1024];
static void parseinfo (char *pi, const char *key)
{
	char s[1024];
	char *dip;
	char keyok;
	char kstate;

	strcpy (s,pi);

	s[strlen(s)+1]=0;

	dip=s;
	keyok=0;
	kstate=0;
	while (*dip)
	{
		char *d2p=dip;
		while (*dip)
		{
			char t;

			d2p++;
			t=*d2p;
			if (t==';' || t==' ' || !t)
			{
				*d2p=0;
				if (!kstate)
				{
					keyok = !strcmp(dip,key);
					kstate= 1;
				} else if (keyok)
				{
					strcat(reglist,dip);
					strcat(reglist," ");
				}

				if (t==';')
					kstate=keyok=0;

				do
					dip=++d2p;
				while (*dip && (*dip==' ' || *dip==';'));
			}
		}
	}
}

char *_lnkReadInfoReg(const char *key)
{
	int i;
	char **pi;
	*reglist=0;
	for (i=0;i<loadlist_n;i++)
		if ((pi=dlsym(loadlist[i].handle, "dllinfo")))
			parseinfo(*pi, key);
	if (*reglist)
		reglist[strlen(reglist)-1]=0;
	return reglist;
}


char *lnkReadInfoReg(const int id, const char *key)
{
	char **pi=0;
	int i;

	*reglist=0;

	for (i=loadlist_n-1;i>=0;i--)
		if (loadlist[i].id==id)
			if ((pi=dlsym(loadlist[i].handle, "dllinfo")))
				parseinfo(*pi, key);
	if (*reglist)
		reglist[strlen(reglist)-1]=0;

	return reglist;
}

static int _lnkDoLoad(const char *file)
{
	if (loadlist_n>=MAXDLLLIST)
	{
		fprintf(stderr, "Too many open shared objects\n");
		return -1;
	}

	if (!(loadlist[loadlist_n].handle=dlopen(file, RTLD_NOW|RTLD_GLOBAL)))
	{
		fprintf(stderr, "%s\n", dlerror());
		return -1;
	}

	loadlist[loadlist_n].id=++handlecounter;

	if (!(loadlist[loadlist_n].info=(struct linkinfostruct *)dlsym(loadlist[loadlist_n].handle, "dllextinfo")))
	{
		fprintf(stderr, "lnkDoLoad(%s): dlsym(dllextinfo): %s\n", file, dlerror());
		return -1;
	}

	{
		struct stat st;
		if (stat(file, &st))
			st.st_size=0;
		loadlist[loadlist_n].info->size=st.st_size;
	}

	loadlist_n++;

	return handlecounter;
}

static int lnkDoLoad(const char *file)
{
	char buffer[PATH_MAX+1];

#ifdef LD_DEBUG
	fprintf(stderr, "Request to load %s\n", file);
#endif
	if ((strlen(cfProgramDir)+strlen(file)+strlen(LIB_SUFFIX))>PATH_MAX)
	{
		fprintf(stderr, "File path to long %s%s%s\n", cfProgramDir, file, LIB_SUFFIX);
		return -1;
	}
	strcat(strcat(strcpy(buffer, cfProgramDir), file), LIB_SUFFIX);
#ifdef LD_DEBUG
	fprintf(stderr, "Attempting to load %s\n", buffer);
#endif
	return _lnkDoLoad(buffer);
}

#ifdef HAVE_QSORT
static int cmpstringp(const void *p1, const void *p2)
{
	return strcmp(*(char * const *)p1, *(char * const *)p2);
}
#else
static void bsort(char **files, int n)
{
	/* old classic bouble sort */
	int m;
	int c;

	if (n<=1)
		return;
	c=1;
	n--;
	while (c)
	{
		c=0;
		for (m=0;m<n;m++)
		{
			if (strcmp(files[m], files[m+1])>0)
			{
				char *t = files[m];
				files[m]=files[m+1];
				files[m+1]=t;
				c=1;
			}
		}
	}
}
#endif

int lnkLinkDir(const char *dir)
{
	DIR *d;
	struct dirent *de;
	char *filenames[1024];
	char buffer[PATH_MAX+1];
	int files=0;
	int n;
	if (!(d=opendir(dir)))
	{
		perror("opendir()");
		return -1;
	}
	while ((de=readdir(d)))
	{
		size_t len=strlen(de->d_name);
		if (len<strlen(LIB_SUFFIX))
			continue;
		if (strcmp(de->d_name+len-strlen(LIB_SUFFIX), LIB_SUFFIX))
			continue;
		if (files>=1024)
		{
			fprintf(stderr, "lnkLinkDir: Too many libraries in directory %s\n", dir);
			closedir(d);
			return -1;
		}
		filenames[files++]=strdup(de->d_name);
	}
	closedir(d);
	if (!files)
		return 0;
#ifdef HAVE_QSORT
	qsort(filenames, files, sizeof(char *), cmpstringp);
#else
	bsort(filenames, files);
#endif
	for (n=0;n<files;n++)
	{
		if (snprintf(buffer, sizeof(buffer), "%s%s", dir, filenames[n])>=(signed)(sizeof(buffer)-1))
		{
			fprintf(stderr, "lnkLinkDir: path too long %s%s\n", dir, filenames[n]);
			for (;n<files;n++)
				free(filenames[n]);
			return -1;
		}
		if (_lnkDoLoad(buffer)<0)
		{
			for (;n<files;n++)
				free(filenames[n]);
			return -1;
		}
		free(filenames[n]);
	}
	return 0; /* all okey */
}

int lnkLink(const char *files)
{
	int retval=0;
	char *tmp=strdup(files);
	char *tmp2=tmp;
	char *tmp3;
	while ((tmp3=strtok(tmp2, " ")))
	{
		tmp2=NULL;
		tmp3[strlen(tmp3)]=0;
		if (strlen(tmp3))
		{
			if ((retval=lnkDoLoad(tmp3))<0)
				break;
		}
	}
	free(tmp);
	return retval;
}

void lnkFree(const int id)
{
	int i;

	if (!id)
	{
		for (i=loadlist_n-1;i>=0;i--)
		{
#ifndef NO_DLCLOSE
			if (loadlist[i].handle) /* this happens for static plugins */
				dlclose(loadlist[i].handle);
#endif
		}
		loadlist_n=0;
	} else {
		for (i=loadlist_n-1;i>=0;i--)
			if (loadlist[i].id==id)
			{
#ifndef NO_DLCLOSE
				if (loadlist[i].handle) /* this happens for static plugins */
					dlclose(loadlist[i].handle);
#endif
				memmove(&loadlist[i], &loadlist[i+1], (MAXDLLLIST-i-1)*sizeof(loadlist[0]));
				loadlist_n--;
				return;
			}
	}
}

#ifdef SUPPORT_STATIC_PLUGINS
static void lnkLoadStatics(void)
{
	struct linkinfostruct *iterator = &staticdlls;
	while (iterator->name)
	{
		#ifdef LD_DEBUG
		fprintf(stderr, "[lnk] Adding static module: \"%s\"\n", iterator->name);
		#endif

		if (loadlist_n>=MAXDLLLIST)
		{
			fprintf(stderr, "[lnk] Too many open shared objects\n");
			return; /* this is really not reachable, but nice to test... */
		}

		loadlist[loadlist_n].handle=NULL;

		loadlist[loadlist_n].id=++handlecounter;

		loadlist[loadlist_n].info=iterator;
		
		loadlist[loadlist_n].info->size=0;

		loadlist_n++;

		iterator++;
	}
	return;
}
#endif

void lnkInit(void)
{
	loadlist_n=0;
	handlecounter=0;
	memset(loadlist, 0, sizeof(loadlist));
#ifdef SUPPORT_STATIC_PLUGINS
	lnkLoadStatics();
#endif
}

void *lnkGetSymbol(const int id, const char *name)
{
	int i;
	if (!id)
	{
		void *retval;
		for (i=loadlist_n-1;i>=0;i--)
			if ((retval=dlsym(loadlist[i].handle, name)))
				return retval;
	} else
		for (i=loadlist_n-1;i>=0;i--)
			if (loadlist[i].id==id)
				return dlsym(loadlist[i].handle, name);
	return NULL;
}

int lnkCountLinks(void)
{
	return loadlist_n;
}

int lnkGetLinkInfo(struct linkinfostruct *l, int index)
{
	if (index<0)
		return 0;
	if (index>=loadlist_n)
		return 0;
	if (!loadlist[index].info)
		return 0;
	memcpy(l, loadlist[index].info, sizeof(struct linkinfostruct));
	return 1;
}

