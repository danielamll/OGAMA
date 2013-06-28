﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace OgamaDao.Model.Rta
{
    
    public class RtaCategory : OgamaDao.Model.BaseModel
    {
        public virtual string name { get; set; }
        public virtual string description { get; set; }
        public virtual RtaCategory parent { get; set; }
        public virtual Boolean show { get; set; }
        public virtual RtaSettings fkRtaSettings { get; set; }
        
        public List<RtaCategory> children = new List<RtaCategory>();
        private List<RtaEvent> events = new List<RtaEvent>();

        public RtaCategory()
        {

        }


        public void Add(RtaEvent rtaEvent)
        {
            this.events.Add(rtaEvent);
        }

        public void SetRtaEvents(List<RtaEvent> events)
        {
            this.events = events;
        }

        public List<RtaEvent> GetRtaEvents()
        {
            return this.events;
        }

        public void SetActive(bool active)
        {
            this.show = active;
            for (int i = 0; i < children.Count; i++)
            {
                RtaCategory child = children.ElementAt(i);
                child.SetActive(active);
            }
        }

        public RtaCategory(string name)
        {
            this.name = name;
        }
        public List<RtaCategory> getChildren()
        {
            return this.children;
        }

        public void Add(RtaCategory category)
        {
            this.children.Add(category);
            category.parent = this;
        }

        public void Remove(RtaCategory category)
        {
            this.children.Remove(category);
        }
    }

}
