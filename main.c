#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <unistd.h>

#define VERSION       "v1.0.0"

char  *file_name;
typedef gboolean (*option_func) (char **);
typedef struct 
{
	const char  *option_cmd;
	option_func  option_exec;  
}docker_opt;

static gboolean docker_from_opt (char **line);
static gboolean docker_copy_opt (char **line);
static gboolean docker_add_opt (char **line);
static gboolean docker_expose_opt (char **line);
static gboolean docker_cmd_opt (char **line);
static docker_opt array_docker_opt [20] =
{
    {"FROM",    docker_from_opt},
    {"COPY",    docker_copy_opt},
    {"ADD",     docker_add_opt},
    {"EXPOSE",  docker_expose_opt},
    {"CMD",     docker_cmd_opt},
    {NULL,      NULL}
};

static void exit_process (void)
{
	g_free (file_name);
	exit (0);
}
void output_error_message (const char *header,const char *message,...)
{
    va_list args;
    char   *file_data;
    char    buf[1024];
    int     len;

    fprintf(stderr,"Version %s author %s\r\n",VERSION,"zhuyaliang");
    va_start(args,message);
    vsprintf(buf,message, args);
    va_end(args);
    fprintf(stderr,"%s:\r\n %s\r\n",header,buf);
	
	exit_process ();
}
static gboolean docker_save_image (const char *image,char **standard_error)
{
	const gchar *argv[6];
    gint         status;
    GError      *error = NULL;
	char        *shell_docker;
	char        *standard_output;
	char        *compress_name;

	shell_docker = g_find_program_in_path("docker");

	if (shell_docker == NULL)
	{
		output_error_message ("Build","There is no \"docker\" command");
	}
	
	compress_name = g_strdup_printf("%s.tar",file_name);
	argv[0] = shell_docker;
	argv[1] = "save";
    argv[2] = image;
	argv[3] = "-o";
	argv[4] = compress_name;
	argv[5] = NULL;
	
    if (!g_spawn_sync (NULL, (gchar**)argv, NULL, 0, NULL, NULL,NULL,standard_error, &status, &error))
        goto ERROR;

    if (!g_spawn_check_exit_status (status, &error))
        goto ERROR;
	
	g_free (compress_name);
	return TRUE;
ERROR:

	g_free (compress_name);
	return FALSE;
}
static gboolean is_image_exists (const char *image)
{
	const gchar *argv[5];
    gint         status;
    GError      *error = NULL;
	char        *shell_docker;
	char        *standard_error;
	char        *standard_output;


	shell_docker = g_find_program_in_path("docker");
	if (shell_docker == NULL)
	{
		output_error_message ("Build","There is no \"docker\" command");
	}
	argv[0] = shell_docker;
	argv[1] = "images";
	argv[2] = "--format";
	argv[3] = "{{.Repository}}:{{.Tag}}";
	argv[4] = NULL;

	if (!g_spawn_sync (NULL, (gchar**)argv, NULL, 0, NULL, NULL,&standard_output,&standard_error, &status, &error))
		goto ERROR;
	if (!g_spawn_check_exit_status (status, &error))
		goto ERROR;
	if (strlen (standard_output) == 0 )
		goto ERROR;
	if (strstr(standard_output,image) == NULL)
		goto ERROR;
	
	return TRUE;
ERROR:
	return FALSE;
}
static gboolean extract_docker_image (char **standard_error)
{
	const gchar *argv[6];
    gint         status;
    GError      *error = NULL;
	char        *shell_tar;
	char        *standard_output;
	char        *compress_dir;
	char        *compress_name;
	

	compress_dir = g_strdup_printf("%s-dir",file_name);
	compress_name = g_strdup_printf("%s.tar",file_name);
	
	mkdir(compress_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
	shell_tar = g_find_program_in_path("tar");

	if (shell_tar == NULL)
	{
		output_error_message ("Build","There is no \"tar\" command");
	}
	argv[0] = shell_tar;
	argv[1] = "-xf";
	argv[2] = compress_name;
	argv[3] = "-C";
	argv[4] = compress_dir;
	argv[5] = NULL;


	if (!g_spawn_sync (NULL, (gchar**)argv, NULL, 0, NULL, NULL,&standard_output,standard_error, &status, &error))
		goto ERROR;

	if (!g_spawn_check_exit_status (status, &error))
		goto ERROR;

	remove (compress_name);
	g_free (compress_dir);
	g_free (compress_name);
	return TRUE;
ERROR:
	remove (compress_name);
	g_free (compress_dir);
	g_free (compress_name);
	
	return FALSE;
}
static gboolean docker_from_opt (char **line)
{
	const gchar *argv[5];
    gint         status;
    GError      *error = NULL;
	char        *shell_docker;
	char        *standard_error;
	char        *standard_output;

	shell_docker = g_find_program_in_path("docker");

	if (shell_docker == NULL)
	{
		output_error_message ("Build","There is no \"docker\" command");
	}
	
	line[1][strlen (line[1]) -1] = '\0';
	if (!is_image_exists (line[1]))
	{
		argv[0] = shell_docker;
		argv[1] = "pull";
		argv[2] = line[1];
		argv[3] = NULL;
	
		if (!g_spawn_sync (NULL, (gchar**)argv, NULL, 0, NULL, NULL,&standard_output,&standard_error, &status, &error))
			goto ERROR;

		if (!g_spawn_check_exit_status (status, &error))
			goto ERROR;
	}

	if (!docker_save_image (line[1],&standard_error))
		goto ERROR;
	if (!extract_docker_image (&standard_error))
		goto ERROR;

	return TRUE;
ERROR:
	output_error_message ("Build",standard_error);

}
static gboolean docker_copy_opt (char **line)
{

}
static gboolean docker_add_opt (char **line)
{

}
static gboolean docker_expose_opt (char **line)
{

}
static gboolean docker_cmd_opt (char **line)
{

}
static gboolean parse_docker_file (const char *image_name,const char *image_tag)
{
	FILE *fp;
	char  buf[1024] = { 0 };
	char **line;
	int   i = 0;
	
	fp = fopen ("./docker.ini","r");
	if (fp == NULL)
	{
		output_error_message ("Build","Error opening profile(docker.ini)");
	}
	while((fgets(buf,1024,fp)) != NULL)
	{
		if (strlen (buf) <= 2 )
		{
			continue;
		}
		line = g_strsplit (buf," ",-1);
		while (array_docker_opt[i].option_cmd != NULL)
		{
			if (g_strcmp0 (line[0],array_docker_opt[i].option_cmd) == 0)
			{
				array_docker_opt[i].option_exec (line);
			}
			i++;
		}
		memset (buf,'\0',1024);
	}
}
static int docker_build (const char *new_image)
{
	char **str;
	struct stat st;

	if (!g_str_is_ascii (new_image))
	{
		output_error_message ("Build","Invalid new image name Use standard characters");
	}
	str = g_strsplit (new_image,":",-1);
	if (g_strv_length(str) != 2)
	{
		
		output_error_message ("Build","Parameter format error use [name]:[tag]");
	}
	if (stat("./docker.ini",&st) != 0 )
	{
		output_error_message ("Build","This file (docker.ini) does not exist in the current directory");
	}
	if ( !S_ISREG(st.st_mode) )
	{
		output_error_message ("Build","This file (docker.ini) does not exist in the current directory");
	}
	
	if (parse_docker_file (str[0],str[1]))
	{
		g_strfreev (str);
		return -1;
	}

	g_strfreev (str);
	return 0;
}
static void set_tmp_name (void)
{
	time_t t;
    int    t_stamp;

    t_stamp = time(&t);
    file_name = g_strdup_printf("tmp-%d",t_stamp);

}
int main (int argc,char **argv)
{
	int ret;
	
	set_tmp_name ();
	if (argc < 2)
	{
		output_error_message ("usage","mkdimage:[Push] or [Pull] or [Build]");
	}
	
	if ( g_strcmp0(argv[1],"Push") ==0 )
	{
	
	}
	else if ( g_strcmp0(argv[1],"Pull") == 0 )
	{
	
	}
	else if ( g_strcmp0(argv[1],"Build") == 0 )
	{
		if (argc < 3)
		{
			output_error_message ("Build","docker image name:tag");
		}
		ret = docker_build (argv[2]);	
	}
	else
	{
		output_error_message ("usage","mkdimage:[Push] or [Pull] or [Build]");
	}
	

}
