/*****************************************************************************
 * intf.c : audio output API towards the interface modules
 *****************************************************************************
 * Copyright (C) 2002 VideoLAN
 * $Id: intf.c,v 1.9 2002/12/06 10:10:39 sam Exp $
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>                            /* calloc(), malloc(), free() */
#include <string.h>

#include <vlc/vlc.h>

#include "audio_output.h"
#include "aout_internal.h"


/*
 * Volume management
 *
 * The hardware volume cannot be set if the output module gets deleted, so
 * we must take the mixer lock. The software volume cannot be set while the
 * mixer is running, so we need the mixer lock (too).
 *
 * Here is a schematic of the i_volume range :
 *
 * |------------------------------+---------------------------------------|
 * 0                           pi_soft                                   1024
 *
 * Between 0 and pi_soft, the volume is done in hardware by the output
 * module. Above, the output module will change p_aout->mixer.i_multiplier
 * (done in software). This scaling may result * in cropping errors and
 * should be avoided as much as possible.
 *
 * It is legal to have *pi_soft == 0, and do everything in software.
 * It is also legal to have *pi_soft == 1024, and completely avoid
 * software scaling. However, some streams (esp. A/52) are encoded with
 * a very low volume and users may complain.
 */

/*****************************************************************************
 * aout_VolumeGet : get the volume of the output device
 *****************************************************************************/
int aout_VolumeGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    int i_result;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->mixer.b_error )
    {
        /* The output module is destroyed. */
        vlc_mutex_unlock( &p_aout->mixer_lock );
        msg_Err( p_aout, "VolumeGet called without output module" );
        return -1;
    }

    i_result = p_aout->output.pf_volume_get( p_aout, pi_volume );

    vlc_mutex_unlock( &p_aout->mixer_lock );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeSet : set the volume of the output device
 *****************************************************************************/
int aout_VolumeSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    int i_result;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->mixer.b_error )
    {
        /* The output module is destroyed. */
        vlc_mutex_unlock( &p_aout->mixer_lock );
        msg_Err( p_aout, "VolumeSet called without output module" );
        return -1;
    }

    i_result = p_aout->output.pf_volume_set( p_aout, i_volume );

    vlc_mutex_unlock( &p_aout->mixer_lock );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeInfos : get the boundary pi_soft
 *****************************************************************************/
int aout_VolumeInfos( aout_instance_t * p_aout, audio_volume_t * pi_soft )
{
    int i_result;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->mixer.b_error )
    {
        /* The output module is destroyed. */
        vlc_mutex_unlock( &p_aout->mixer_lock );
        msg_Err( p_aout, "VolumeInfos called without output module" );
        return -1;
    }

    i_result = p_aout->output.pf_volume_infos( p_aout, pi_soft );

    vlc_mutex_unlock( &p_aout->mixer_lock );
    return i_result;
}

/*****************************************************************************
 * aout_VolumeUp : raise the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function.
 *****************************************************************************/
int aout_VolumeUp( aout_instance_t * p_aout, int i_nb_steps,
                   audio_volume_t * pi_volume )
{
    int i_result;
    audio_volume_t i_volume;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->mixer.b_error )
    {
        /* The output module is destroyed. */
        vlc_mutex_unlock( &p_aout->mixer_lock );
        msg_Err( p_aout, "VolumeUp called without output module" );
        return -1;
    }

    if ( p_aout->output.pf_volume_get( p_aout, &i_volume ) )
    {
        vlc_mutex_unlock( &p_aout->mixer_lock );
        return -1;
    }

    i_volume += AOUT_VOLUME_STEP * i_nb_steps;
    if ( i_volume > 1024 ) i_volume = 1024;

    i_result = p_aout->output.pf_volume_set( p_aout, i_volume );

    vlc_mutex_unlock( &p_aout->mixer_lock );

    if ( pi_volume != NULL ) *pi_volume = i_volume;
    return i_result;
}

/*****************************************************************************
 * aout_VolumeDown : lower the output volume
 *****************************************************************************
 * If pi_volume != NULL, *pi_volume will contain the volume at the end of the
 * function.
 *****************************************************************************/
int aout_VolumeDown( aout_instance_t * p_aout, int i_nb_steps,
                     audio_volume_t * pi_volume )
{
    int i_result;
    audio_volume_t i_volume;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->mixer.b_error )
    {
        /* The output module is destroyed. */
        vlc_mutex_unlock( &p_aout->mixer_lock );
        msg_Err( p_aout, "VolumeUp called without output module" );
        return -1;
    }

    if ( p_aout->output.pf_volume_get( p_aout, &i_volume ) )
    {
        vlc_mutex_unlock( &p_aout->mixer_lock );
        return -1;
    }

    if ( i_volume < AOUT_VOLUME_STEP * i_nb_steps )
        i_volume = 0;
    else
        i_volume -= AOUT_VOLUME_STEP * i_nb_steps;

    i_result = p_aout->output.pf_volume_set( p_aout, i_volume );

    vlc_mutex_unlock( &p_aout->mixer_lock );

    if ( pi_volume != NULL ) *pi_volume = i_volume;
    return i_result;
}

/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for software-only scaling.
 */

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeSoftInit( aout_instance_t * p_aout )
{
    int i_volume;

    p_aout->output.pf_volume_infos = aout_VolumeSoftInfos;
    p_aout->output.pf_volume_get = aout_VolumeSoftGet;
    p_aout->output.pf_volume_set = aout_VolumeSoftSet;

    i_volume = config_GetInt( p_aout, "volume" );
    if ( i_volume < 0 )
    {
        i_volume = AOUT_VOLUME_DEFAULT;
    }
    else if ( i_volume > AOUT_VOLUME_MAX )
    {
        i_volume = AOUT_VOLUME_MAX;
    }

    aout_VolumeSoftSet( p_aout, (audio_volume_t)i_volume );
}

/* Placeholder for pf_volume_infos(). */
int aout_VolumeSoftInfos( aout_instance_t * p_aout, audio_volume_t * pi_soft )
{
    *pi_soft = 0;
    return 0;
}

/* Placeholder for pf_volume_get(). */
int aout_VolumeSoftGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    *pi_volume = p_aout->output.i_volume;
    return 0;
}


/* Placeholder for pf_volume_set(). */
int aout_VolumeSoftSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    aout_MixerMultiplierSet( p_aout, (float)i_volume / AOUT_VOLUME_DEFAULT );
    p_aout->output.i_volume = i_volume;
    return 0;
}

/*
 * The next functions are not supposed to be called by the interface, but
 * are placeholders for unsupported scaling.
 */

/* Meant to be called by the output plug-in's Open(). */
void aout_VolumeNoneInit( aout_instance_t * p_aout )
{
    p_aout->output.pf_volume_infos = aout_VolumeNoneInfos;
    p_aout->output.pf_volume_get = aout_VolumeNoneGet;
    p_aout->output.pf_volume_set = aout_VolumeNoneSet;
}

/* Placeholder for pf_volume_infos(). */
int aout_VolumeNoneInfos( aout_instance_t * p_aout, audio_volume_t * pi_soft )
{
    return -1;
}

/* Placeholder for pf_volume_get(). */
int aout_VolumeNoneGet( aout_instance_t * p_aout, audio_volume_t * pi_volume )
{
    return -1;
}

/* Placeholder for pf_volume_set(). */
int aout_VolumeNoneSet( aout_instance_t * p_aout, audio_volume_t i_volume )
{
    return -1;
}


/*
 * Pipelines management
 */

/*****************************************************************************
 * aout_Restart : re-open the output device and rebuild the input and output
 *                pipelines
 *****************************************************************************
 * This function is used whenever the parameters of the output plug-in are
 * changed (eg. selecting S/PDIF or PCM).
 *****************************************************************************/
int aout_Restart( aout_instance_t * p_aout )
{
    int i;
    vlc_bool_t b_error = 0;

    vlc_mutex_lock( &p_aout->mixer_lock );

    if ( p_aout->i_nb_inputs == 0 )
    {
        vlc_mutex_unlock( &p_aout->mixer_lock );
        msg_Err( p_aout, "no decoder thread" );
        return -1;
    }

    /* Lock all inputs. */
    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        vlc_mutex_lock( &p_aout->pp_inputs[i]->lock );
        aout_InputDelete( p_aout, p_aout->pp_inputs[i] );
    }

    aout_MixerDelete( p_aout );

    /* Re-open the output plug-in. */
    aout_OutputDelete( p_aout );

    if ( aout_OutputNew( p_aout, &p_aout->pp_inputs[0]->input ) == -1 )
    {
        /* Release all locks and report the error. */
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
        vlc_mutex_unlock( &p_aout->mixer_lock );
        return -1;
    }

    if ( aout_MixerNew( p_aout ) == -1 )
    {
        aout_OutputDelete( p_aout );
        for ( i = 0; i < p_aout->i_nb_inputs; i++ )
        {
            vlc_mutex_unlock( &p_aout->pp_inputs[i]->lock );
        }
        vlc_mutex_unlock( &p_aout->mixer_lock );
        return -1;
    }

    /* Re-open all inputs. */
    for ( i = 0; i < p_aout->i_nb_inputs; i++ )
    {
        aout_input_t * p_input = p_aout->pp_inputs[i];

        b_error |= aout_InputNew( p_aout, p_input );
        vlc_mutex_unlock( &p_input->lock );
    }

    vlc_mutex_unlock( &p_aout->mixer_lock );

    return b_error;
}

/*****************************************************************************
 * aout_FindAndRestart : find the audio output instance and restart
 *****************************************************************************
 * This is used for callbacks of the configuration variables, and we believe
 * that when those are changed, it is a significant change which implies
 * rebuilding the audio-device and audio-channels variables.
 *****************************************************************************/
void aout_FindAndRestart( vlc_object_t * p_this )
{
    aout_instance_t * p_aout = vlc_object_find( p_this, VLC_OBJECT_AOUT,
                                                FIND_ANYWHERE );

    if ( p_aout == NULL ) return;

    if ( var_Type( p_aout, "audio-device" ) >= 0 )
    {
        var_Destroy( p_aout, "audio-device" );
    }
    if ( var_Type( p_aout, "audio-channels" ) >= 0 )
    {
        var_Destroy( p_aout, "audio-channels" );
    }

    aout_Restart( p_aout );
    vlc_object_release( p_aout );
}

/*****************************************************************************
 * aout_ChannelsRestart : change the audio device or channels and restart
 *****************************************************************************/
int aout_ChannelsRestart( vlc_object_t * p_this, const char * psz_variable,
                          vlc_value_t old_value, vlc_value_t new_value,
                          void * unused )
{
    aout_instance_t * p_aout = (aout_instance_t *)p_this;

    if ( !strcmp( psz_variable, "audio-device" ) )
    {
        /* This is supposed to be a significant change and supposes
         * rebuilding the channel choices. */
        if ( var_Type( p_aout, "audio-channels" ) >= 0 )
        {
            var_Destroy( p_aout, "audio-channels" );
        }
    }
    aout_Restart( p_aout );
    return 0;
}
