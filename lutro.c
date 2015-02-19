#include "lutro.h"
#include "runtime.h"
#include "file/file_path.h"
#include "compat/strl.h"

#include "graphics.h"
#include "input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <assert.h>
#include <libgen.h>
#include <unistd.h>

static lua_State *L;
lutro_settings_t settings = {
   .width = 320,
   .height = 240,
   .pitch = 0,
   .framebuffer = NULL,
   .input_cb = NULL
};

static int lutro_core_preload(lua_State *L)
{
   lutro_ensure_global_table(L, "lutro");

   return 1;
}

static void init_settings(lua_State *L)
{
   lutro_ensure_global_table(L, "lutro");

   lua_newtable(L);

   lua_pushnumber(L, settings.width);
   lua_setfield(L, -2, "width");

   lua_pushnumber(L, settings.height);
   lua_setfield(L, -2, "height");

   lua_setfield(L, -2, "settings");

   lua_pop(L, 1);
}

void lutro_init()
{
   L = luaL_newstate();
   luaL_openlibs(L);

   init_settings(L);

   lutro_preload(L, lutro_core_preload, "lutro");
   lutro_preload(L, lutro_graphics_preload, "lutro.graphics");
   lutro_preload(L, lutro_input_preload, "lutro.input");

   lua_getglobal(L, "require");
   lua_pushstring(L, "lutro");
   lua_call(L, 1, 1);

   lua_getglobal(L, "require");
   lua_pushstring(L, "lutro.graphics");
   lua_call(L, 1, 1);

   lua_getglobal(L, "require");
   lua_pushstring(L, "lutro.input");
   lua_call(L, 1, 1);

   // remove this if undefined references to lutro.* happen
   lua_pop(L, 2);
}

void lutro_deinit()
{
   lua_close(L);
}

int lutro_set_package_path( lua_State* L, const char* path )
{
   const char *cur_path;
   char new_path[PATH_MAX_LENGTH];
   lua_getglobal(L, "package");
   lua_getfield(L, -1, "path");
   cur_path = lua_tostring( L, -1);
   strlcpy(new_path, cur_path, sizeof(new_path));
   strlcat(new_path, path, sizeof(new_path));
   lua_pop(L, 1);
   lua_pushstring(L, new_path) ;
   lua_setfield(L, -2, "path");
   lua_pop(L, 1);
   return 1;
}

int lutro_load(const char *path)
{
   char package_path[PATH_MAX_LENGTH];
   strlcpy(package_path, ";", sizeof(package_path));
   strlcat(package_path, path, sizeof(package_path));
   path_basedir(package_path);
   strlcat(package_path, "?.lua;", sizeof(package_path));
   lutro_set_package_path(L, package_path);

   if(luaL_dofile(L, path))
   {
       fprintf(stderr, "%s\n", lua_tostring(L, -1));
       lua_pop(L, 1);

       return 0;
   }

   lua_getglobal(L, "lutro");

   char game_dir[PATH_MAX_LENGTH];
   strlcpy(game_dir, path, sizeof(game_dir));
   path_basedir(game_dir);
   lua_pushstring(L, game_dir);
   lua_setfield(L, -2, "path");

   lua_pushnumber(L, 0);
   lua_setfield(L, -2, "camera_x");

   lua_pushnumber(L, 0);
   lua_setfield(L, -2, "camera_y");

   lua_getfield(L, -1, "conf");

   if (lua_isnoneornil(L, -1))
   {
      puts("skipping custom configuration.");
   }
   else
   {
      lua_getfield(L, -2, "settings");

      if(lua_pcall(L, 1, 0, 0))
      {
         fprintf(stderr, "%s\n", lua_tostring(L, -1));
         lua_pop(L, 1);

         return 0;
      }

      lua_getfield(L, -1, "settings");

      lua_getfield(L, -1, "width");
      settings.width = lua_tointeger(L, -1);
      lua_remove(L, -1);

      lua_getfield(L, -1, "height");
      settings.height = lua_tointeger(L, -1);
      lua_remove(L, -1);
   }

   lua_pop(L, 1); // either lutro.settings or lutro.conf

   lutro_graphics_init();

   lua_getfield(L, -1, "load");

   if (lua_isnoneornil(L, -1))
   {
      puts("skipping custom initialization.");
   }
   else
   {
      if(lua_pcall(L, 0, 0, 0))
      {
         fprintf(stderr, "%s\n", lua_tostring(L, -1));
         lua_pop(L, 1);

         return 0;
      }
   }

   lua_pop(L, 1);

   return 1;
}

int lutro_run(double delta)
{
   lua_getglobal(L, "lutro");

   lua_getfield(L, -1, "update");

   if (lua_isfunction(L, -1))
   {
      lua_pushnumber(L, delta);
      if(lua_pcall(L, 1, 0, 0))
      {
         fprintf(stderr, "%s\n", lua_tostring(L, -1));
         lua_pop(L, 1);

         return 0;
      }
   } else {
      lua_pop(L, 1);
   }

   lua_getfield(L, -1, "draw");
   if (lua_isfunction(L, -1))
   {
      if(lua_pcall(L, 0, 0, 0))
      {
         fprintf(stderr, "%s\n", lua_tostring(L, -1));
         lua_pop(L, 1);

         return 0;
      }
   } else {
      lua_pop(L, 1);
   }

   lua_pop(L, 1);

   return 1;
}