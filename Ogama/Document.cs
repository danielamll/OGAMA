// <copyright file="Document.cs" company="FU Berlin">
// ******************************************************
// OGAMA - open gaze and mouse analyzer 
// Copyright (C) 2015 Dr. Adrian Vo?k?hler  
// ------------------------------------------------------------------------
// This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
// This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
// You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
// **************************************************************
// </copyright>
// <author>Adrian Vo?k?hler</author>
// <email>adrian@ogama.net</email>

namespace Ogama
{
  using System;
  using System.Collections.Generic;
  using System.Collections.Specialized;
  using System.ComponentModel;
  using System.Data.SqlClient;
  using System.Data.SQLite;
  using System.Drawing;
  using System.IO;
  using System.Runtime.InteropServices;
  using System.Windows.Forms;

  using DbAccess;

  using GTNetworkClient;

  using Microsoft.SqlServer.Management.Common;
  using Microsoft.SqlServer.Management.Smo;
  using Ogama.DataSet;
  using Ogama.ExceptionHandling;
  using Ogama.MainWindow.ContextPanel;
  using Ogama.MainWindow.Dialogs;
  using Ogama.Modules.Common.Tools;
  using Ogama.Modules.Common.Types;
  using Ogama.Properties;
  using VectorGraphics.Elements;
  using VectorGraphics.Elements.ElementCollections;

  /// <summary>
  /// This class defines an OGAMA document.
  /// Is a singleton class type with the current document as static member.
  /// It also has a modified flag and an experiment settings class member along with
  /// the current selected trial and subject state.
  /// </summary>
  /// <remarks>To retrieve the current active document and its experiment settings
  /// call <code>Document.ActiveDocument</code> resp.
  /// <code>Document.ActiveDocument.ExperimentSettings</code>.</remarks>
  public class Document
  {
    ///////////////////////////////////////////////////////////////////////////////
    // Defining Constants                                                        //
    ///////////////////////////////////////////////////////////////////////////////
    #region CONSTANTS

    /// <summary>
    /// The maximum height of a thumb in the slide design module.
    /// </summary>
    public const int SLIDEDESIGNTHUMBMAXHEIGHT = 100;

    /// <summary>
    /// The maximum width of a thumb in the slide design module.
    /// </summary>
    public const int SLIDEDESIGNTHUMBMAXWIDTH = 150;

    /// <summary>
    /// The maximum height of a thumb in the context panel.
    /// </summary>
    public const int CONTEXTPANELTHUMBMAXHEIGHT = 96;

    /// <summary>
    /// The maximum width of a thumb in the context panel.
    /// </summary>
    public const int CONTEXTPANELTHUMBMAXWIDTH = 128;

    #endregion //CONSTANTS

    ///////////////////////////////////////////////////////////////////////////////
    // Defining Variables, Enumerations, Events                                  //
    ///////////////////////////////////////////////////////////////////////////////
    #region FIELDS

    /// <summary>
    /// Current active document
    /// </summary>
    private static Document activeDocument;

    /// <summary>
    /// Experiment settings class member with screensize and database path and 
    /// connection string.
    /// </summary>
    private ExperimentSettings experimentSettings;

    /// <summary>
    /// Class that contains the current active Subject, TrialID and TrialImagefile
    /// </summary>
    private SelectionsState selectionState;

    /// <summary>
    /// Dataset assigned with this document.
    /// </summary>
    private SQLiteOgamaDataSet dataSet;

    /// <summary>
    /// Flag. True, if document parameters were modified.
    /// </summary>
    private bool isModified;

    #endregion //FIELDS

    ///////////////////////////////////////////////////////////////////////////////
    // Construction and Initializing methods                                     //
    ///////////////////////////////////////////////////////////////////////////////
    #region CONSTRUCTION

    /// <summary>
    /// Initializes a new instance of the Document class.
    /// </summary>
    public Document()
    {
      activeDocument = this;
      this.isModified = false;
      this.experimentSettings = new ExperimentSettings();
      this.selectionState = new SelectionsState();
    }

    #endregion //CONSTRUCTION

    ///////////////////////////////////////////////////////////////////////////////
    // Defining Properties                                                       //
    ///////////////////////////////////////////////////////////////////////////////
    #region PROPERTIES

    /// <summary>
    /// Gets or sets current active document.
    /// </summary>
    /// <value>A <see cref="Document"/> with the current document.</value>
    public static Document ActiveDocument
    {
      get { return activeDocument; }
      set { activeDocument = value; }
    }

    /// <summary>
    /// Gets or sets a value indicating whether the document is modified.
    /// True, if there are changes in the document settings.
    /// </summary>
    /// <value>A <see cref="bool"/> which is <strong>true</strong>, when the document
    /// has unsaved changes.</value>
    public bool Modified
    {
      get { return this.isModified; }
      set { this.isModified = value; }
    }

    /// <summary>
    /// Gets or sets experiment settings.
    /// </summary>
    /// <value>A <see cref="ExperimentSettings"/> for the current document.</value>
    public ExperimentSettings ExperimentSettings
    {
      get { return this.experimentSettings; }
      set { this.experimentSettings = value; }
    }

    /// <summary>
    /// Gets a <see cref="RectangleF"/> with the presentation screen size
    /// located at 0,0.
    /// </summary>
    /// <value>A <see cref="RectangleF"/> with the presentation screen size
    /// located at 0,0..</value>
    public RectangleF PresentationSizeRectangle
    {
      get
      {
        return new RectangleF(
          0,
          0,
          this.experimentSettings.WidthStimulusScreen,
          this.experimentSettings.HeightStimulusScreen);
      }
    }

    /// <summary>
    /// Gets or sets the presentation screen size.
    /// </summary>
    public Size PresentationSize
    {
      get
      {
        return new Size(
          this.experimentSettings.WidthStimulusScreen,
          this.experimentSettings.HeightStimulusScreen);
      }

      set
      {
        this.experimentSettings.WidthStimulusScreen = value.Width;
        this.experimentSettings.HeightStimulusScreen = value.Height;
      }
    }

    /// <summary>
    /// Gets or sets the current active Subject, TrialID and TrialImagefile
    /// </summary>
    /// <value>A <see cref="SelectionsState"/> for the current document.</value>
    public SelectionsState SelectionState
    {
      get { return this.selectionState; }
      set { this.selectionState = value; }
    }

    /// <summary>
    /// Gets or sets assigned dataset.
    /// </summary>
    /// <value>The <see cref="SQLiteOgamaDataSet"/> for the current document.</value>
    public SQLiteOgamaDataSet DocDataSet
    {
      get { return this.dataSet; }
      set { this.dataSet = value; }
    }

    #endregion //PROPERTIES

    ///////////////////////////////////////////////////////////////////////////////
    // Public methods                                                            //
    ///////////////////////////////////////////////////////////////////////////////
    #region PUBLICMETHODS

    /// <summary>
    /// Destruction. Clear member fields and close database connections.
    /// </summary>
    public void CleanUp()
    {
      //SqlConnection connectionString = new SqlConnection(Document.ActiveDocument.ExperimentSettings.ServerConnectionString);
      //ServerConnection connection = new ServerConnection(connectionString);
      //Server sqlServer = new Server(connection);
      try
      {
        //// If there are open connections set offline and online to kill all 
        //// active connections, they are not used anymore.
        //int connections = sqlServer.GetActiveDBConnectionCount(Document.ActiveDocument.ExperimentSettings.Name);
        //if (connections > 0)
        //{
        //  string query = "ALTER DATABASE \"" + Document.ActiveDocument.ExperimentSettings.Name +
        //   "\" SET OFFLINE WITH ROLLBACK IMMEDIATE;";
        //  Queries.ExecuteSQLCommand(query);
        //  query = "ALTER DATABASE \"" + Document.ActiveDocument.ExperimentSettings.Name +
        //    "\" SET ONLINE;";
        //  Queries.ExecuteSQLCommand(query);
        //}

        // Close database connection
        if (this.dataSet != null)
        {
          this.dataSet.Dispose();
        }

        //// Detach database from user instance
        //if (sqlServer.Databases.Contains(Document.ActiveDocument.ExperimentSettings.Name))
        //{
        //  sqlServer.DetachDatabase(Document.ActiveDocument.ExperimentSettings.Name, true);
        //}
      }
      catch (Exception ex)
      {
        ExceptionMethods.HandleException(ex);
      }

      activeDocument = null;
      this.isModified = false;
      this.experimentSettings = null;
      this.selectionState = null;
    }

    #endregion //PUBLICMETHODS

    ///////////////////////////////////////////////////////////////////////////////
    // Inherited methods                                                         //
    ///////////////////////////////////////////////////////////////////////////////
    #region OVERRIDES
    #endregion //OVERRIDES

    ///////////////////////////////////////////////////////////////////////////////
    // Eventhandler                                                              //
    ///////////////////////////////////////////////////////////////////////////////
    #region EVENTS

    ///////////////////////////////////////////////////////////////////////////////
    // Eventhandler for UI, Menu, Buttons, Toolbars etc.                         //
    ///////////////////////////////////////////////////////////////////////////////
    #region WINDOWSEVENTHANDLER
    #endregion //WINDOWSEVENTHANDLER

    ///////////////////////////////////////////////////////////////////////////////
    // Eventhandler for Custom Defined Events                                    //
    ///////////////////////////////////////////////////////////////////////////////
    #region CUSTOMEVENTHANDLER
    #endregion //CUSTOMEVENTHANDLER

    #endregion //EVENTS

    ///////////////////////////////////////////////////////////////////////////////
    // Methods and Eventhandling for Background tasks                            //
    ///////////////////////////////////////////////////////////////////////////////
    #region BACKGROUNDWORKER
    #endregion //BACKGROUNDWORKER

    ///////////////////////////////////////////////////////////////////////////////
    // Methods for doing main class job                                          //
    ///////////////////////////////////////////////////////////////////////////////
    #region METHODS

    /// <summary>
    /// Tries to load document of given file and updates recent files list.
    /// </summary>
    /// <param name="filePath">A <see cref="string"/> with the full path to .oga xml file.</param>
    /// <param name="addToRecentFiles">Flag. <strong>True</strong>, if file should be
    /// added to recent files list.</param>
    /// <param name="splash">The loading document splash screens background worker,
    /// needed for interrupting splash when SQL connection fails.</param>
    /// <returns><strong>True</strong>,if successful, otherwise
    /// <strong>false</strong>.</returns>
    public bool LoadDocument(
      string filePath,
      bool addToRecentFiles,
      BackgroundWorker splash)
    {
      if (addToRecentFiles)
      {
        RecentFilesList.List.Add(filePath);
      }

      // Check folder layout
      if (!this.CheckCorrectSubFolderLayout(filePath))
      {
        return false;
      }

      // Deserialize the experiment settings
      if (!this.LoadSettingsFromFile(filePath, splash))
      {
        return false;
      }

      // Set thumb sizes to presentation proportions
      this.RescaleThumbSizes();

      // Load Database
      if (!this.LoadSQLData(splash))
      {
        return false;
      }

      // Write ogama version string into the settings.
      this.ExperimentSettings.UpdateVersion();

      return true;
    }

    /// <summary>
    /// Tries to save document and updates recent files list.
    /// </summary>
    /// <param name="filePath">A <see cref="string"/> with the full path to .oga xml file.</param>
    /// <param name="splash">The loading document splash screens background worker,
    /// needed for interrupting splash when SQL connection fails.</param>
    /// <returns><strong>True</strong>,if successful, otherwise
    /// <strong>false</strong>.</returns>
    public bool SaveDocument(string filePath, BackgroundWorker splash)
    {
      if (!this.SaveSettingsToFile(filePath))
      {
        return false;
      }

      RecentFilesList.List.Add(filePath);

      // Reset modified flag, because saving succeeded.
      this.Modified = false;

      return true;
    }

    /// <summary>
    /// Creates OgamaDataSet and fills it with data from SQL Databasefile 
    /// given bei <see cref="Properties.ExperimentSettings.DatabaseConnectionString"/>.
    /// </summary>
    /// <param name="splash">The loading document splash screens background worker,
    /// needed for interrupting splash when SQL connection fails.</param>
    /// <returns><strong>True</strong>,if successful, otherwise
    /// <strong>false</strong>.</returns>
    public bool LoadSQLData(BackgroundWorker splash)
    {
      // Creates new DataSet
      this.dataSet = new SQLiteOgamaDataSet();

      // Data Source=C:\Users\Adrian\Documents\OgamaExperiments\SlideshowDemo\Database\SlideshowDemo.sdf;Max Database Size=4091
      //bool automaticCorrectMachineNameTried = false;
      //bool automaticResetConnectionTried = false;
      //bool logRebuild = false;

      if (!File.Exists(this.experimentSettings.DatabaseSQLiteFile))
      {
        var result = InformationDialog.Show(
          "Upgrade Database",
          "We need to convert the SQL Database to SQLite. Otherwise Ogama will not be able to open the data. Please confirm the conversion process",
          true,
          MessageBoxIcon.Question);
        switch (result)
        {
          case DialogResult.Cancel:
            return false;
          case DialogResult.Yes:
            // Show loading splash screen if it is not running
            var bgwConvert = new BackgroundWorker();
            bgwConvert.DoWork += this.bgwConvert_DoWork;
            bgwConvert.WorkerSupportsCancellation = true;
            bgwConvert.RunWorkerAsync();

            // Remove the log, cause it may disable the conversion process
            // if it comes from another storage location
            if (File.Exists(this.experimentSettings.DatabaseLDFFile))
            {
              File.Delete(this.experimentSettings.DatabaseLDFFile);
            }

            this.ConvertToSQLiteDatabase();

            bgwConvert.CancelAsync();

            InformationDialog.Show(
              "Conversion done.",
              "The database was converted to sqlite format. The original source was not removed.",
              false,
              MessageBoxIcon.Information);
            break;
          case DialogResult.No:
            break;
        }
      }

      // Check for existing database
      if (!File.Exists(this.experimentSettings.DatabaseSQLiteFile))
      {
        string message = "The experiments database: " + Environment.NewLine +
          this.experimentSettings.DatabaseSQLiteFile + Environment.NewLine +
          "does not exist. This error could not be automically resolved." +
          Environment.NewLine + "Please move the database to the above location, " +
          "or create a new experiment.";
        ExceptionMethods.ProcessErrorMessage(message);
        return false;
      }


      // Show loading splash screen if it is not running
      if (splash != null && !splash.IsBusy)
      {
        splash.RunWorkerAsync();
      }

      //  SqlConnection connectionString = new SqlConnection(Document.ActiveDocument.ExperimentSettings.ServerConnectionString);
      //  ServerConnection connection = new ServerConnection(connectionString);
      //  Server sqlServer = new Server(connection);
      //Attach:
      //  try
      //  {
      //    StringCollection sc = new StringCollection();
      //    sc.Add(Document.ActiveDocument.ExperimentSettings.DatabaseMDFFile);
      //    if (logRebuild)
      //    {
      //      if (File.Exists(Document.ActiveDocument.ExperimentSettings.DatabaseLDFFile))
      //      {
      //        File.Delete(Document.ActiveDocument.ExperimentSettings.DatabaseLDFFile);
      //      }
      //    }
      //    else
      //    {
      //      sc.Add(Document.ActiveDocument.ExperimentSettings.DatabaseLDFFile);
      //    }

      //    // Attach database file
      //    if (!sqlServer.Databases.Contains(Document.ActiveDocument.ExperimentSettings.Name))
      //    {
      //      sqlServer.AttachDatabase(
      //        Document.ActiveDocument.ExperimentSettings.Name,
      //        sc,
      //        logRebuild ? AttachOptions.RebuildLog : AttachOptions.None);
      //    }

      //    sqlServer.ConnectionContext.Disconnect();
      //    logRebuild = false;
      //  }
      //  catch (Exception ex)
      //  {
      //    // Check for the SQLError 9004, which indicates a failure 
      //    // in the log file, which can be fixed by rebuilding the log.
      //    if (ex.InnerException != null)
      //    {
      //      if (ex.InnerException.InnerException != null)
      //      {
      //        if (ex.InnerException.InnerException is SqlException)
      //        {
      //          if (((SqlException)ex.InnerException.InnerException).Number == 9004)
      //          {
      //            logRebuild = true;
      //          }
      //        }
      //      }
      //    }

      //    if (!logRebuild)
      //    {
      //      string message = @"The following error occured: " + ex.Message + Environment.NewLine + Environment.NewLine +
      //        @"If it is the following: 'Failed to generate a user instance of SQL Server due to a failure in starting the process for the user instance. The connection will be closed.'" +
      //        @"Please delete the folder 'Local Settings\Application Data\Microsoft\Microsoft SQL Server Data\" + Document.ActiveDocument.ExperimentSettings.SqlInstanceName +
      //        "' in WinXP or " +
      //        @"'AppData\Local\Microsoft\Microsoft SQL Server Data\" + Document.ActiveDocument.ExperimentSettings.SqlInstanceName + "' on Vista and Windows 7 and try again.";
      //      ExceptionMethods.ProcessMessage("SQL Server connection failed.", message);
      //      ExceptionMethods.ProcessUnhandledException(ex);
      //      if (splash != null && splash.IsBusy)
      //      {
      //        splash.CancelAsync();
      //      }

      //      return false;
      //    }
      //  }

      //  // Go back and rebuild the log file.
      //  if (logRebuild)
      //  {
      //    goto Attach;
      //  }

      // Test connection
      using (var conn = new SQLiteConnection(Document.ActiveDocument.ExperimentSettings.DatabaseConnectionString))
      {
        try
        {
          conn.Open();
        }
        catch (Exception ex)
        {
          //if (splash != null && splash.IsBusy)
          //{
          //  splash.CancelAsync();
          //}

          //if (!automaticResetConnectionTried)
          //{
          //  this.experimentSettings.CustomConnectionString = string.Empty;
          //  this.isModified = true;
          //  automaticResetConnectionTried = true;
          //  goto Test;
          //}

          //if (!automaticCorrectMachineNameTried)
          //{
          //  string currentConnectionString = this.experimentSettings.DatabaseConnectionString;
          //  int firstEqualSign = currentConnectionString.IndexOf('=', 0);
          //  int firstBackslash = currentConnectionString.IndexOf('\\', 0);
          //  string machineName = currentConnectionString.Substring(firstEqualSign + 1, firstBackslash - firstEqualSign - 1);
          //  string currentMachineName = Environment.MachineName;
          //  currentConnectionString = currentConnectionString.Replace(machineName + "\\", currentMachineName + "\\");
          //  this.experimentSettings.CustomConnectionString = currentConnectionString;
          //  automaticCorrectMachineNameTried = true;
          //  this.isModified = true;
          //  goto Test;
          //}

          string message = "Connection to SQLite database failed." + Environment.NewLine +
            "Take a careful look at the database file to attach and its path. " +
            "The .db file should be named the same as the experiment .oga file and located in the "
            + "Database subfolder of the experiment." + Environment.NewLine + "The error message is: "
            + Environment.NewLine + ex.Message + Environment.NewLine;
          ExceptionMethods.ProcessErrorMessage(message);

          //SQLConnectionDialog connectionDialog = new SQLConnectionDialog();
          //connectionDialog.ConnectionString = this.experimentSettings.DatabaseConnectionString;
          //if (connectionDialog.ShowDialog() == DialogResult.OK)
          //{
          //  this.experimentSettings.CustomConnectionString = connectionDialog.ConnectionString;
          //  this.isModified = true;
          //}
          //else
          //{
          //  return false;
          //}

          //goto Test;
        }
        finally
        {
          try
          {
            conn.Close();
          }
          catch (Exception)
          {
          }
        }
      }

      if (splash != null && !splash.IsBusy)
      {
        splash.RunWorkerAsync();
      }

      Application.DoEvents();

      // Loads tables and Data from SQL file into DataSet
      if (this.dataSet.LoadData(splash))
      {
        return true;
      }

      return false;
    }

    private void bgwConvert_DoWork(object sender, DoWorkEventArgs e)
    {
      // Get the BackgroundWorker that raised this event.
      BackgroundWorker worker = sender as BackgroundWorker;

      var newSplash = new ConvertDatabaseSplash();
      newSplash.Worker = worker;
      newSplash.ShowDialog();
    }

    /// <summary>
    /// Load experiment settings from settings file given in parameters.
    /// </summary>
    /// <param name="filePath">A <see cref="string"/> with the path 
    /// and filename to the settings file.</param>
    /// <param name="splash">The loading document splash screens background worker,
    /// needed for interrupting splash when SQL connection fails.</param>
    /// <returns><strong>True</strong>,if successful, otherwise
    /// <strong>false</strong>.</returns>
    public bool LoadSettingsFromFile(string filePath, BackgroundWorker splash)
    {
      try
      {
        this.experimentSettings = ExperimentSettings.Deserialize(filePath);

        // If deserialization succeeded update possible copied experiment folders
        // references.
        if (this.experimentSettings != null)
        {
          string filename = Path.GetFileNameWithoutExtension(filePath);
          string directory = Path.GetDirectoryName(filePath);

          // Setup document path and name
          this.experimentSettings.DocumentPath = directory;

          // Set path properties of slideshow elements
          this.experimentSettings.SlideShow.UpdateExperimentPathOfResources(
            this.experimentSettings.SlideResourcesPath);

          // Write version information
          if (!this.experimentSettings.UpdateVersion())
          {
            return false;
          }

          this.experimentSettings.Name = filename;

          // Check if database file is correct located in the connection string
          // abort if user aborts.
          if (!this.experimentSettings.CheckDatabasePath())
          {
            return false;
          }

          return true;
        }
      }
      catch (WarningException)
      {
        // This is the case when the user cancels the upgrading
        return false;
      }
      catch (Exception ex)
      {
        ExceptionMethods.HandleException(ex);
      }

      return false;
    }

    /// <summary>
    /// Save experiment settings to settings file given in parameters.
    /// </summary>
    /// <param name="filePath">A <see cref="string"/> with the path 
    /// and filename to the settings file.</param>
    /// <returns><strong>True</strong>,if successful, otherwise
    /// <strong>false</strong>.</returns>
    public bool SaveSettingsToFile(string filePath)
    {
      try
      {
        if (ExperimentSettings.Serialize(this.experimentSettings, filePath))
        {
          return true;
        }
      }
      catch (Exception ex)
      {
        ExceptionMethods.HandleException(ex);
      }

      return false;
    }

    /// <summary>
    /// This method scales the static thumb sizes for the slides and context panel
    /// to the proportions given in the <see cref="ExperimentSettings"/>.
    /// </summary>
    private void RescaleThumbSizes()
    {
      // Scale thumbs to proportions of presentation screen
      int screenWidth = this.experimentSettings.WidthStimulusScreen;
      int screenHeight = this.experimentSettings.HeightStimulusScreen;
      float screenRatio = (float)screenHeight / (float)screenWidth;

      // Calculate context panel thumb size
      int newContextPanelThumbHeight = (int)(CONTEXTPANELTHUMBMAXWIDTH * screenRatio);
      int newContextPanelThumbWidth = (int)(CONTEXTPANELTHUMBMAXHEIGHT / screenRatio);

      if (newContextPanelThumbWidth > CONTEXTPANELTHUMBMAXWIDTH)
      {
        ContextPanel.ContextPanelThumbSize = new Size(CONTEXTPANELTHUMBMAXWIDTH, newContextPanelThumbHeight);
      }
      else
      {
        ContextPanel.ContextPanelThumbSize = new Size(newContextPanelThumbWidth, CONTEXTPANELTHUMBMAXHEIGHT);
      }

      // Calculate slide design thumb size
      int newSlideThumbHeight = (int)(SLIDEDESIGNTHUMBMAXWIDTH * screenRatio);
      int newSlideThumbWidth = (int)(SLIDEDESIGNTHUMBMAXHEIGHT / screenRatio);
      if (newSlideThumbWidth > SLIDEDESIGNTHUMBMAXWIDTH)
      {
        Slide.SlideDesignThumbSize = new Size(SLIDEDESIGNTHUMBMAXWIDTH, newSlideThumbHeight);
      }
      else
      {
        Slide.SlideDesignThumbSize = new Size(newSlideThumbWidth, SLIDEDESIGNTHUMBMAXHEIGHT);
      }
    }

    /// <summary>
    /// This method parses the substructure of the opened experiment,
    /// to ensure correct layout. By default it is only used
    /// to update Ogama 0.X folder layout structure to new format.
    /// </summary>
    /// <param name="filePath">A <see cref="string"/> with the full path to .oga xml file.</param>
    /// <returns><strong>True</strong> if everything is ok, or layout is successful converted,
    /// if user cancelled update returns <strong>false</strong>.</returns>
    private bool CheckCorrectSubFolderLayout(string filePath)
    {
      string experimentPath = Path.GetDirectoryName(filePath);
      string experimentName = Path.GetFileNameWithoutExtension(filePath);
      string databaseFile = Path.Combine(experimentPath, experimentName + ".mdf");
      string databaseLogFile = Path.Combine(experimentPath, experimentName + "_log.ldf");
      DirectoryInfo dirInfoStimuli = new DirectoryInfo(experimentPath);

      // Get directories.
      List<string> directoryNames = new List<string>();
      foreach (DirectoryInfo directory in dirInfoStimuli.GetDirectories())
      {
        directoryNames.Add(directory.Name);
      }

      if (!directoryNames.Contains("SlideResources"))
      {
        Directory.CreateDirectory(Path.Combine(experimentPath, "SlideResources"));
      }

      if (!directoryNames.Contains("Thumbs"))
      {
        Directory.CreateDirectory(Path.Combine(experimentPath, "Thumbs"));
      }

      if (File.Exists(databaseFile) && !directoryNames.Contains("Database"))
      {
        string message = "It seems that you are opening an Ogama 0.X experiment file." +
          "Because the database is in the same folder as the experiment file and there is no " +
          "'Database' directory." +
          Environment.NewLine +
          "Ogama is now updating you folder layout, before converting the experiment to the new version.";

        DialogResult result = InformationDialog.Show(
          "Ogama V0.X experiment found, update ?",
          message,
          true,
          MessageBoxIcon.Information);

        switch (result)
        {
          case DialogResult.Cancel:
          case DialogResult.No:
            return false;
          case DialogResult.Yes:
            break;
        }

        // Add folder
        string databasePath = Path.Combine(experimentPath, "Database");
        if (!directoryNames.Contains("Database"))
        {
          Directory.CreateDirectory(databasePath);
        }

        // Delete thumb files
        string thumbFile = Path.Combine(experimentPath, experimentName + "Images.thumbs");
        if (File.Exists(thumbFile))
        {
          File.Delete(thumbFile);
        }

        string thumbsListFile = Path.Combine(experimentPath, experimentName + "Images.thumbsList");
        if (File.Exists(thumbsListFile))
        {
          File.Delete(thumbsListFile);
        }

        // Moving database
        string newDatabaseFile = Path.Combine(databasePath, experimentName + ".mdf");
        if (File.Exists(databaseFile))
        {
          File.Move(databaseFile, newDatabaseFile);
        }

        string newDatabaseLogFile = Path.Combine(databasePath, experimentName + "_log.ldf");
        if (File.Exists(databaseLogFile))
        {
          File.Move(databaseLogFile, newDatabaseLogFile);
        }
      }

      return true;
    }

    /// <summary>
    /// Converts to sq lite database.
    /// </summary>
    private void ConvertToSQLiteDatabase()
    {
      var sqlConnString = @"Server=.\SQLExpress;AttachDbFilename=" + this.experimentSettings.DatabaseMDFFile + ";Database=" +
       this.experimentSettings.Name + ";Integrated Security=True;User Instance=True;Connection Timeout=30";
      string sqlitePath = this.experimentSettings.DatabaseSQLiteFile;
      SqlServerToSQLite.ConvertSqlServerDatabaseToSQLiteFile(sqlConnString, sqlitePath);
    }

    #endregion //METHODS

    ///////////////////////////////////////////////////////////////////////////////
    // Small helping Methods                                                     //
    ///////////////////////////////////////////////////////////////////////////////
    #region HELPER
    #endregion //HELPER
  }
}
