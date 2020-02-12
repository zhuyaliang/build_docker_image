#include <stdio.h>
#include <string.h>
#include <gtk/gtk.h>
#include <sys/stat.h>
#include <unistd.h>
#include <glib/gstdio.h>
#include <json-c/json.h>
#include <sys/types.h>
#include <fcntl.h>

#define VERSION       "v1.0.0"

char  *file_name;
char  *compress_dir;
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
	char shell_cmd[1024] = { 0 };

	g_free (file_name);
	if (compress_dir != NULL)
	{
		sprintf (shell_cmd,"rm -rf %s",compress_dir);
		system (shell_cmd);
		g_free (compress_dir);
		
	}
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
	g_free (compress_name);
	return TRUE;
ERROR:
	remove (compress_name);
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
static gboolean copy_file (const char *source,const char *dest,char **standard_error)
{
	FILE *in,*out;
	char buff[1024];
	int  len;

	in = fopen(source,"r+");
	if (in == NULL)
	{
		*standard_error = "copy file error";
		return FALSE;
	}
	out = fopen(dest,"w+");
	if (out == NULL)
	{
		fclose (in);
		*standard_error = "copy file error";
		return FALSE;
	}
	while(len = fread(buff,1,sizeof(buff),in))
	{
		fwrite(buff,1,len,out);
	}
	fclose (in);
	fclose (out);
	return TRUE;

}
static gboolean create_version_file (char *version_file,char **standard_error)
{
	FILE *fp;

	fp = fopen(version_file,"w+");
	if (fp == NULL)
	{
		*standard_error = "create docker image VERSION file faild";
		return FALSE;
	}
	fwrite("1.0",1,3,fp);
	fclose (fp);

	return TRUE;

}
static char *get_sha256sum_value (const char *layer_name)
{
}
static char *create_image_layer (char *copy_tmp)
{
	char *work_dir;
	char  shell_cmd[1024] = { 0 };
	FILE *fp;
    char  buffer[65];

	work_dir = g_get_current_dir();
	
	chdir (copy_tmp);
	sprintf (shell_cmd,"tar -cf layer.tar * --remove-file");
	system (shell_cmd);
	
    fp = popen("sha256sum layer.tar", "r");
    fgets(buffer, sizeof(buffer), fp);
    pclose(fp);
	chdir (work_dir);
	
	return g_strdup (buffer);
	
}
static char *get_current_local_time (void)
{
	GDateTime *dt;
	char *time;

	dt = g_date_time_new_now_local ();
	time =  g_date_time_format(dt, "%FT%T");

	return time;
}
static void add_rootfs_diff_ids (json_object *js,char *sha256_value)
{
	char json_data[100] = { 0 };
    json_object *js_sha256_ids;
	enum json_type type;
	
	sprintf (json_data,"sha256:%s",sha256_value);
	json_object_object_foreach(js, key, value)  /*Passing through every array element*/
    {
        type = json_object_get_type(value);
		if (type == json_type_array)
        {
			js_sha256_ids = json_object_new_string(json_data);
			json_object_array_add (value,js_sha256_ids);
		}
    }
}
static void change_config_json (const char *config_file,const char *option,char *sha256_value)
{
	int fd;
	char jsonbuff[10240] = { 0 };
	json_object	*js,*js_object;
	enum json_type type;
	char *time;
	char create_by[1024] = { 0 };
	char        *json_file;
	
	json_file = g_build_filename (compress_dir,config_file,NULL);
	fd = open (json_file,O_RDWR);
	read (fd,jsonbuff,10240);
	js = json_tokener_parse(jsonbuff);
	js_object = json_object_new_object();
	
	time = get_current_local_time();
	sprintf (create_by,"/bin/sh -c #(nop) %s",option);	
	json_object_object_foreach(js, key, value)  
    {
        type = json_object_get_type(value);
        if (g_strcmp0(key,"history") == 0)
        {
			json_object_object_add(js_object, "created", json_object_new_string(time));  
			json_object_object_add(js_object, "created_by", json_object_new_string(create_by));  
			json_object_object_add(js_object, "empty_layer", json_object_new_boolean(1));  
			json_object_array_add (value,js_object);
		}
		else if (g_strcmp0(key,"rootfs") == 0)
		{
			add_rootfs_diff_ids (value,sha256_value);
		}

    }
		
	json_object_to_file (json_file,js);
	g_free (json_file);
}
static gboolean change_json_file (char *sha256_value,const char *option)
{
	json_object	*js;
	char        *json_file;
	enum json_type type;
    json_object *js_sha256,*js_array;
	char jsonbuff[10240] = { 0 };
	int fd;
	char json_data[100] = { 0 };
	const char *config_file;

//
	json_file = g_build_filename (compress_dir,"manifest.json",NULL);
	fd = open (json_file,O_RDWR);
	read (fd,jsonbuff,10240);
	
	jsonbuff[0] = ' ';
	jsonbuff[strlen(jsonbuff) - 1] = ' ';
	js = json_tokener_parse(jsonbuff);

	sprintf (json_data,"%s/layer.tar",sha256_value);
    json_object_object_foreach(js, key, value)  /*Passing through every array element*/
    {
        type = json_object_get_type(value);
		if (type == json_type_array)
        {
			js_sha256 = json_object_new_string(json_data);
			json_object_array_add (value,js_sha256);
		}
        if (g_strcmp0(key,"Config") == 0)
        {
			config_file = json_object_get_string (value);
        }
    }
	js_array = json_object_new_array();
	json_object_array_add (js_array,js);
	close (fd);
	json_object_to_file (json_file,js_array);
	g_free (json_file);

//
	change_config_json (config_file,option,sha256_value);
	
}
static gboolean docker_copy_opt (char **line)
{
	char *copy_tmp;
	char *dest_dir;
	char *dest_file;
	struct stat st;
	char *version_file;
	char *standard_error;
	char *new_dir_name;
	char *sha256_value;

	copy_tmp = g_build_filename (compress_dir,"copy_tmp", NULL);
	mkdir (copy_tmp, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
	
	if (stat(line[1],&st) != 0 )
	{
		g_free (copy_tmp);
		output_error_message ("Build","source file %s does not exist",line[1]);
	}
	if ( !S_ISREG(st.st_mode) )
	{
		g_free (copy_tmp);
		output_error_message ("Build","source file %s not a regular file",line[1]);
	}
	
	line[2][strlen (line[1]) -1] = '\0';
	dest_dir = g_build_filename (copy_tmp,line[2], NULL);
	mkdir (dest_dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH); 
	dest_file = g_build_filename (dest_dir,line[1], NULL);
	
	if (!copy_file (line[1],dest_file,&standard_error))
	{
		goto ERROR;
		
	}
	
	sha256_value = create_image_layer (copy_tmp);
	version_file = g_build_filename (copy_tmp,"VERSION", NULL);
	if (!create_version_file (version_file,&standard_error))
	{
		goto ERROR;
	}
	
	new_dir_name = g_build_filename (compress_dir,sha256_value, NULL);
	g_rename (copy_tmp,new_dir_name);
	
	change_json_file (sha256_value,"COPY file");
	g_free (dest_file);
	g_free (copy_tmp);
	g_free (dest_dir);
	g_free (sha256_value);
	g_free (version_file);
	return TRUE;
ERROR:
	g_free (copy_tmp);
	g_free (dest_file);
	g_free (dest_dir);
	g_free (sha256_value);
	g_free (version_file);

	output_error_message ("Build",standard_error);
}
static gboolean docker_add_opt (char **line)
{

}
static void add_image_tcp_port (json_object *js,GPtrArray *port_array)
{
	enum json_type type;
	json_object *js_object;
	json_object *js_port;
	json_object *js_empty;
	char         json_data[100] = { 0 };
	
	js_object = json_object_new_object(); 
	
	json_object_object_foreach(js, key, value)
    {
        if (g_strcmp0(key,"ExposedPorts") == 0)
        {
			json_object_object_del (js,key);
        }
	}
	js_port = json_object_new_object(); 
	js_empty = json_object_new_object(); 
	for (guint i = 0; i < port_array->len; i++)
    {
        gchar *port = g_ptr_array_index (port_array, i);
		if (port[strlen(port) -1] == '\n')
		{
			port[strlen(port) -1] = '\0';
		}
		sprintf (json_data,"%s/tcp",port);
		json_object_object_add(js_port,json_data,js_empty);
		memset (json_data,'\0',100);
	}
	json_object_object_add(js,"ExposedPorts",js_port);
}
static void change_json_expose_port (const char *config_file,GPtrArray *port_array)
{
	int fd;
	char jsonbuff[10240] = { 0 };
	json_object	*js;
	char        *json_file;
	
	json_file = g_build_filename (compress_dir,config_file,NULL);
	fd = open (json_file,O_RDWR);
	read (fd,jsonbuff,10240);
	
	js = json_tokener_parse(jsonbuff);
	json_object_object_foreach(js, key, value)  
    {
		if (g_strcmp0(key,"config") == 0)
		{
			add_image_tcp_port (value,port_array);
		}

    }
	json_object_to_file (json_file,js);
	g_free (json_file);

}
static gboolean docker_expose_opt (char **line)
{
	json_object	*js;
	int          i = 1;
	char        *json_file;
	char jsonbuff[10240] = { 0 };
	int fd;
	const char *config_file;

	g_autoptr(GPtrArray) port_array;

	port_array = g_ptr_array_new ();
	while (line[i] != NULL)
	{
		g_ptr_array_add (port_array,line[i]);
		i++;
	}
	if (i == 1)
	{
		output_error_message ("Build","Please add port number");
	}
	json_file = g_build_filename (compress_dir,"manifest.json",NULL);
	fd = open (json_file,O_RDWR);
	read (fd,jsonbuff,10240);
	
	jsonbuff[0] = ' ';
	jsonbuff[strlen(jsonbuff) - 1] = ' ';
	js = json_tokener_parse(jsonbuff);

    json_object_object_foreach(js, key, value)  /*Passing through every array element*/
    {
        if (g_strcmp0(key,"Config") == 0)
        {
			config_file = json_object_get_string (value);
        }
    }
	g_free (json_file);
	
	change_json_expose_port (config_file,port_array);

	return TRUE;
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
				break;
			}
			i++;
		}
		memset (buf,'\0',1024);
	}
	fclose (fp);
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
