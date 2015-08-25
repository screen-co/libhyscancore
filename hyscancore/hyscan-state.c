/*!
 * \file hyscan-state.c
 *
 * \brief Исходный файл класса состояния HyScan
 * \author Andrei Fadeev (andrei@webcontrol.ru)
 * \date 2015
 * \license Проприетарная лицензия ООО "Экран"
 *
*/

#include "hyscan-state.h"


enum { PROP_O,
       PROP_DB,
       PROP_PROJECT_NAME,
       PROP_TRACK_NAME,
       PROP_PRESET_NAME,
       PROP_PROFILE_NAME
};


enum { SIGNAL_DB_CHANGED,
       SIGNAL_PROJECT_NAME_CHANGED,
       SIGNAL_TRACK_NAME_CHANGED,
       SIGNAL_PRESET_NAME_CHANGED,
       SIGNAL_PROFILE_NAME_CHANGED,
       SIGNAL_LAST
};


typedef struct HyScanSatePriv {                  // Внутренние данные объекта.

  HyScanDB                  *db;                 // Указатель на интерфейс базы данных.

  gchar                     *project_name;       // Название текущего проекта.
  gchar                     *track_name;         // Название текущего галса.
  gchar                     *preset_name;        // Название текущей группы настроек обработки проекта.

  gchar                     *profile_name;       // Название текущего профиля задачи.

} HyScanSatePriv;


#define HYSCAN_STATE_GET_PRIVATE( obj ) ( G_TYPE_INSTANCE_GET_PRIVATE( ( obj ), G_TYPE_HYSCAN_STATE, HyScanSatePriv ) )


static void hyscan_state_set_property( HyScanSate *state, guint prop_id, const GValue *value, GParamSpec *pspec );
static void hyscan_state_get_property( HyScanSate *state, guint prop_id, GValue *value, GParamSpec *pspec );
static void hyscan_state_object_finalize( HyScanSate *state );

static guint hyscan_state_signals[ SIGNAL_LAST ] = { 0 };


G_DEFINE_TYPE( HyScanSate, hyscan_state, G_TYPE_OBJECT );


static void hyscan_state_init( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  priv->db = NULL;
  priv->project_name = NULL;
  priv->track_name = NULL;
  priv->preset_name = NULL;
  priv->profile_name = NULL;

}


static void hyscan_state_class_init( HyScanSateClass *klass )
{

  GObjectClass *this_class = G_OBJECT_CLASS( klass );

  this_class->set_property = hyscan_state_set_property;
  this_class->get_property = hyscan_state_get_property;
  this_class->finalize = hyscan_state_object_finalize;

  g_object_class_install_property( this_class, PROP_DB,
    g_param_spec_object( "db", "HyScanDB", "HyScanDB intreface", G_TYPE_HYSCAN_DB, G_PARAM_READABLE | G_PARAM_WRITABLE ) );

  g_object_class_install_property( this_class, PROP_PROJECT_NAME,
    g_param_spec_string( "project-name", "Project name", "HyScan DB project name", NULL, G_PARAM_READABLE | G_PARAM_WRITABLE ) );

  g_object_class_install_property( this_class, PROP_TRACK_NAME,
    g_param_spec_string( "track-name", "Track name", "HyScan DB track name", NULL, G_PARAM_READABLE | G_PARAM_WRITABLE ) );

  g_object_class_install_property( this_class, PROP_PRESET_NAME,
    g_param_spec_string( "preset-name", "Preset name", "HyScan DB project preset name", NULL, G_PARAM_READABLE | G_PARAM_WRITABLE ) );

  g_object_class_install_property( this_class, PROP_PROFILE_NAME,
    g_param_spec_string( "profile-name", "Profile name", "HyScan profile name", NULL, G_PARAM_READABLE | G_PARAM_WRITABLE ) );

  hyscan_state_signals[ SIGNAL_DB_CHANGED ] =
    g_signal_new( "db-changed", G_TYPE_HYSCAN_STATE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1, G_TYPE_OBJECT );

  hyscan_state_signals[ SIGNAL_PROJECT_NAME_CHANGED ] =
    g_signal_new( "project-changed", G_TYPE_HYSCAN_STATE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING );

  hyscan_state_signals[ SIGNAL_TRACK_NAME_CHANGED ] =
    g_signal_new( "track-changed", G_TYPE_HYSCAN_STATE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING );

  hyscan_state_signals[ SIGNAL_PRESET_NAME_CHANGED ] =
    g_signal_new( "preset-changed", G_TYPE_HYSCAN_STATE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING );

  hyscan_state_signals[ SIGNAL_PROFILE_NAME_CHANGED ] =
    g_signal_new( "profile-changed", G_TYPE_HYSCAN_STATE, G_SIGNAL_RUN_LAST, 0, NULL, NULL,
                  g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING );

  g_type_class_add_private( klass, sizeof( HyScanSatePriv ) );

}


static void hyscan_state_set_property( HyScanSate *state, guint prop_id, const GValue *value, GParamSpec *pspec )
{

  switch ( prop_id )
    {

    case PROP_DB:
      hyscan_state_set_db( state, g_value_get_object( value ) );
      break;

    case PROP_PROJECT_NAME:
      hyscan_state_set_project_name( state, g_value_get_string( value ) );
      break;

    case PROP_TRACK_NAME:
      hyscan_state_set_track_name( state, g_value_get_string( value ) );
      break;

    case PROP_PRESET_NAME:
      hyscan_state_set_preset_name( state, g_value_get_string( value ) );
      break;

    case PROP_PROFILE_NAME:
      hyscan_state_set_project_name( state, g_value_get_string( value ) );
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( state, prop_id, pspec );
      break;

    }

}


static void hyscan_state_get_property( HyScanSate *state, guint prop_id, GValue *value, GParamSpec *pspec )
{

  switch ( prop_id )
    {

    case PROP_DB:
      g_value_set_object( value, hyscan_state_get_db( state ) );
      break;

    case PROP_PROJECT_NAME:
      g_value_set_string( value, hyscan_state_get_project_name( state ) );
      break;

    case PROP_TRACK_NAME:
      g_value_set_string( value, hyscan_state_get_track_name( state ) );
      break;

    case PROP_PRESET_NAME:
      g_value_set_string( value, hyscan_state_get_preset_name( state ) );
      break;

    case PROP_PROFILE_NAME:
      g_value_set_string( value, hyscan_state_get_profile_name( state ) );
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID( state, prop_id, pspec );
      break;

    }

}


// Деструктор объекта.
static void hyscan_state_object_finalize( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  g_clear_object( &priv->db );

  g_clear_pointer( &priv->project_name, g_free );
  g_clear_pointer( &priv->track_name, g_free );
  g_clear_pointer( &priv->preset_name, g_free );
  g_clear_pointer( &priv->profile_name, g_free );

}


// Функция создаёт новый объект.
HyScanSate *hyscan_state_new( void )
{

  return g_object_new( G_TYPE_HYSCAN_STATE, NULL );

}


//  Функция задаёт новый указатель на интерфейс базы данных.
void hyscan_state_set_db( HyScanSate *state, HyScanDB *db )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  gboolean project_changed = FALSE;
  gboolean track_changed = FALSE;
  gboolean preset_changed = FALSE;

  // Если интерфейсы базы данных совпадают - ничего не делаем.
  if( priv->db == db ) return;

  // Если были установлены имена проекта, галса или группы настроек - посылаем сигнал.
  if( priv->project_name != NULL ) project_changed = TRUE;
  if( priv->track_name != NULL ) track_changed = TRUE;
  if( priv->preset_name != NULL ) preset_changed = TRUE;

  // Закрываем соединение с базой данных.
  g_clear_object( &priv->db );

  // Обнуляем названия проекта, галса и группы настроек.
  g_clear_pointer( &priv->project_name, g_free );
  g_clear_pointer( &priv->track_name, g_free );
  g_clear_pointer( &priv->preset_name, g_free );

  // Запоминаем указатель на интерфейс базы данных.
  priv->db = db;
  if( G_IS_HYSCAN_DB( priv->db ) ) g_object_ref( priv->db );
  else priv->db = NULL;

  // Отправляем сигналы об изменении состояния.
  g_signal_emit( state, hyscan_state_signals[ SIGNAL_DB_CHANGED ], 0, priv->db );
  if( project_changed ) g_signal_emit( state, hyscan_state_signals[ SIGNAL_PROJECT_NAME_CHANGED ], 0, NULL );
  if( track_changed ) g_signal_emit( state, hyscan_state_signals[ SIGNAL_TRACK_NAME_CHANGED ], 0, NULL );
  if( preset_changed ) g_signal_emit( state, hyscan_state_signals[ SIGNAL_PRESET_NAME_CHANGED ], 0, NULL );

}


// Функция возвращает указатель на интерфейс базы данных.
HyScanDB *hyscan_state_get_db( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  return priv->db;

}


// Функция задаёт имя используемого проекта.
void hyscan_state_set_project_name( HyScanSate *state, const gchar *project_name )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  gboolean track_changed = FALSE;
  gboolean preset_changed = FALSE;

  if( priv->db == NULL ) return;

  // Если имена проектов совпадают - ничего не делаем.
  if( g_strcmp0( priv->project_name, project_name ) == 0 ) return;

  // Если были установлены имена галса или группы настроек - посылаем сигнал.
  if( priv->track_name != NULL ) track_changed = TRUE;
  if( priv->preset_name != NULL ) preset_changed = TRUE;

  // Обнуляем названия проекта, галса и группы настроек.
  g_clear_pointer( &priv->project_name, g_free );
  g_clear_pointer( &priv->preset_name, g_free );
  g_clear_pointer( &priv->track_name, g_free );

  // Запоминаем название проекта.
  priv->project_name = g_strdup( project_name );

  // Отправляем сигналы об изменении состояния.
  g_signal_emit( state, hyscan_state_signals[ SIGNAL_PROJECT_NAME_CHANGED ], 0, priv->project_name );
  if( track_changed ) g_signal_emit( state, hyscan_state_signals[ SIGNAL_TRACK_NAME_CHANGED ], 0, NULL );
  if( preset_changed ) g_signal_emit( state, hyscan_state_signals[ SIGNAL_PRESET_NAME_CHANGED ], 0, NULL );

}


// Функция возвращает имя используемого проекта.
const gchar *hyscan_state_get_project_name( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  return priv->project_name;

}


// Функция задаёт имя используемого галса.
void hyscan_state_set_track_name( HyScanSate *state, const gchar *track_name )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  if( priv->db == NULL ) return;

  // Если имена галсов совпадают - ничего не делаем.
  if( g_strcmp0( priv->track_name, track_name ) == 0 ) return;

  // Обнуляем название галса.
  g_clear_pointer( &priv->track_name, g_free );

  // Запоминаем название галса.
  priv->track_name = g_strdup( track_name );

  // Отправляем сигнал об изменении состояния.
  g_signal_emit( state, hyscan_state_signals[ SIGNAL_TRACK_NAME_CHANGED ], 0, priv->track_name );

}


// Функция возвращает имя используемого галса.
const gchar *hyscan_state_get_track_name( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  return priv->track_name;

}


// Функция задаёт имя группы параметров обработки.
void hyscan_state_set_preset_name( HyScanSate *state, const gchar *preset_name )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  if( priv->db == NULL ) return;

  // Если имена группы настроек совпадают - ничего не делаем.
  if( g_strcmp0( priv->preset_name, preset_name ) == 0 ) return;

  // Обнуляем название группы настроек.
  g_clear_pointer( &priv->preset_name, g_free );

  // Запоминаем название группы настроек.
  priv->preset_name = g_strdup( preset_name );

  // Отправляем сигнал об изменении состояния.
  g_signal_emit( state, hyscan_state_signals[ SIGNAL_PRESET_NAME_CHANGED ], 0, priv->preset_name );

}


// Функция возвращает имя группы параметров обработки.
const gchar *hyscan_state_get_preset_name( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  return priv->preset_name;

}


void hyscan_state_set_profile_name( HyScanSate *state, const gchar *profile_name )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  // Если имена профилей задачи совпадают - ничего не делаем.
  if( g_strcmp0( priv->profile_name, profile_name ) == 0 ) return;

  // Обнуляем название профиля задачи.
  g_clear_pointer( &priv->profile_name, g_free );

  // Запоминаем название профиля задачи.
  priv->profile_name = g_strdup( profile_name );

  // Отправляем сигнал об изменении состояния.
  g_signal_emit( state, hyscan_state_signals[ SIGNAL_PROFILE_NAME_CHANGED ], 0, priv->profile_name );

}


const gchar *hyscan_state_get_profile_name( HyScanSate *state )
{

  HyScanSatePriv *priv = HYSCAN_STATE_GET_PRIVATE( state );

  return priv->profile_name;

}
