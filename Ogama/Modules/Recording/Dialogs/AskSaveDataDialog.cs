// <copyright file="AskSaveDataDialog.cs" company="FU Berlin">
// ******************************************************
// OGAMA - open gaze and mouse analyzer 
// Copyright (C) 2015 Dr. Adrian Voßkühler  
// ------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
// **************************************************************
// </copyright>
// <author>Adrian Voßkühler</author>
// <email>adrian@ogama.net</email>

namespace Ogama.Modules.Recording.Dialogs
{
  using System.Windows.Forms;

  /// <summary>
  /// A small popup <see cref="Form"/> to ask for saving the sampling data into 
  /// the database after recording.
  /// </summary>
  public partial class AskSaveDataDialog : Form
  {
    /// <summary>
    /// Initializes a new instance of the AskSaveDataDialog class.
    /// </summary>
    public AskSaveDataDialog()
    {
      this.InitializeComponent();
    }
  }
}